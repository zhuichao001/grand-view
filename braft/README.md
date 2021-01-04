## Node api
- int init(const NodeOptions& options);
- void apply(const Task& task);
- void add_peer(const PeerId& peer, Closure* done);
- void remove_peer(const PeerId& peer, Closure* done);
- void change_peers(const Configuration& new_peers, Closure* done);

## StateMachine api
- void on_apply(::raft::Iterator& iter);
- void on_snapshot_save(SnapshotWriter* writer, Closure* done);
- int on_snapshot_load(SnapshotReader* reader);
- void on_leader_start(int64_t term);
- void on_leader_stop(const butil::Status& status);
- void on_error(const Error& e);
