#ifndef RAY_OBJECT_MANAGER_OBJECT_DIRECTORY_H
#define RAY_OBJECT_MANAGER_OBJECT_DIRECTORY_H

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ray/gcs/client.h"
#include "ray/id.h"
#include "ray/status.h"

namespace ray {

/// Connection information for remote object managers.
struct RemoteConnectionInfo {
  RemoteConnectionInfo() = default;
  RemoteConnectionInfo(const ClientID &id, const std::string &ip_address,
                       uint16_t port_num)
      : client_id(id), ip(ip_address), port(port_num) {}
  ClientID client_id;
  std::string ip;
  uint16_t port;
};

class ObjectDirectoryInterface {
 public:
  ObjectDirectoryInterface() = default;
  virtual ~ObjectDirectoryInterface() = default;

  /// Callbacks for GetInformation.
  using InfoSuccessCallback = std::function<void(const ray::RemoteConnectionInfo &info)>;
  using InfoFailureCallback = std::function<void(ray::Status status)>;

  virtual void RegisterBackend() = 0;

  /// This is used to establish object manager client connections.
  ///
  /// \param client_id The client for which information is required.
  /// \param success_cb A callback which handles the success of this method.
  /// \param fail_cb A callback which handles the failure of this method.
  /// \return Status of whether this asynchronous request succeeded.
  virtual ray::Status GetInformation(const ClientID &client_id,
                                     const InfoSuccessCallback &success_cb,
                                     const InfoFailureCallback &fail_cb) = 0;

  /// Callback for object location notifications.
  using OnLocationsFound = std::function<void(const std::vector<ray::ClientID> &v,
                                              const ray::ObjectID &object_id)>;

  /// Subscribe to be notified of locations (ClientID) of the given object.
  /// The callback will be invoked whenever locations are obtained for the
  /// specified object.
  ///
  /// \param object_id The required object's ObjectID.
  /// \param success_cb Invoked with non-empty list of client ids and object_id.
  /// \return Status of whether subscription succeeded.
  virtual ray::Status SubscribeObjectLocations(const ObjectID &object_id,
                                               const OnLocationsFound &callback) = 0;

  /// Unsubscribe to object location notifications.
  ///
  /// \param object_id The object id invoked with Subscribe.
  /// \return
  virtual ray::Status UnsubscribeObjectLocations(const ObjectID &object_id) = 0;

  /// Report objects added to this node's store to the object directory.
  ///
  /// \param object_id The object id that was put into the store.
  /// \param client_id The client id corresponding to this node.
  /// \param object_info Additional information about the object.
  /// \return Status of whether this method succeeded.
  virtual ray::Status ReportObjectAdded(const ObjectID &object_id,
                                        const ClientID &client_id,
                                        const ObjectInfoT &object_info) = 0;

  /// Report objects removed from this client's store to the object directory.
  ///
  /// \param object_id The object id that was removed from the store.
  /// \param client_id The client id corresponding to this node.
  /// \return Status of whether this method succeeded.
  virtual ray::Status ReportObjectRemoved(const ObjectID &object_id,
                                          const ClientID &client_id) = 0;
};

/// Ray ObjectDirectory declaration.
class ObjectDirectory : public ObjectDirectoryInterface {
 public:
  ObjectDirectory() = default;
  ~ObjectDirectory() override = default;

  void RegisterBackend() override;

  ray::Status GetInformation(const ClientID &client_id,
                             const InfoSuccessCallback &success_callback,
                             const InfoFailureCallback &fail_callback) override;

  ray::Status SubscribeObjectLocations(const ObjectID &object_id,
                                       const OnLocationsFound &callback) override;
  ray::Status UnsubscribeObjectLocations(const ObjectID &object_id) override;

  ray::Status ReportObjectAdded(const ObjectID &object_id, const ClientID &client_id,
                                const ObjectInfoT &object_info) override;
  ray::Status ReportObjectRemoved(const ObjectID &object_id,
                                  const ClientID &client_id) override;
  /// Ray only (not part of the OD interface).
  ObjectDirectory(std::shared_ptr<gcs::AsyncGcsClient> &gcs_client);

  /// ObjectDirectory should not be copied.
  RAY_DISALLOW_COPY_AND_ASSIGN(ObjectDirectory);

 private:
  /// Callbacks associated with a call to GetLocations.
  struct LocationListenerState {
    LocationListenerState(const OnLocationsFound &locations_found_callback)
        : locations_found_callback(locations_found_callback) {}
    /// The callback to invoke when object locations are found.
    OnLocationsFound locations_found_callback;
    /// The current set of known locations of this object.
    std::unordered_set<ClientID> client_ids;
  };

  /// Info about subscribers to object locations.
  std::unordered_map<ObjectID, LocationListenerState> listeners_;
  /// Reference to the gcs client.
  std::shared_ptr<gcs::AsyncGcsClient> gcs_client_;
  /// Map from object ID to the number of times it's been evicted on this
  /// node before.
  std::unordered_map<ObjectID, int> object_evictions_;
};

}  // namespace ray

#endif  // RAY_OBJECT_MANAGER_OBJECT_DIRECTORY_H
