#include <gflags/gflags.h>              // DEFINE_*
#include <brpc/controller.h>            // brpc::Controller
#include <brpc/server.h>                // brpc::Server
#include <braft/raft.h>                 // braft::Node braft::StateMachine
#include <braft/storage.h>              // braft::SnapshotWriter
#include <braft/util.h>                 // braft::AsyncClosureGuard
#include <braft/protobuf_file.h>        // braft::ProtoBufFile
#include "lighter.pb.h"                 // LighterService

DEFINE_bool(check_term, true, "Check if the leader changed to another term");
DEFINE_bool(disable_cli, false, "Don't allow raft_cli access this node");
DEFINE_bool(log_applied_task, false, "Print notice log when a task is applied");
DEFINE_int32(election_timeout_ms, 5000, "Start election in milliseconds if lost leader");
DEFINE_int32(port, 8100, "Listen port of this peer");
DEFINE_int32(snapshot_interval, 30, "Interval between each snapshot");
DEFINE_string(conf, "", "Initial configuration of the replication group");
DEFINE_string(data_path, "./data", "Path of data stored on");
DEFINE_string(group, "Lighter", "Id of the replication group");

namespace grand{

class Lighter;

// Implements Closure which encloses RPC stuff
class IncrbyClosure : public braft::Closure {
public:
    IncrbyClosure(Lighter* lighter, 
                    const IncrbyRequest* request,
                    LighterResponse* response,
                    google::protobuf::Closure* done)
        : _lighter(lighter)
        , _request(request)
        , _response(response)
        , _done(done) {
    }

    ~IncrbyClosure() {}

    const IncrbyRequest* request() const { return _request; }

    LighterResponse* response() const { return _response; }

    void Run();

private:
    Lighter* _lighter;
    const IncrbyRequest* _request;
    LighterResponse* _response;
    google::protobuf::Closure* _done;
};

// Implementation of grand::Lighter as a braft::StateMachine.
class Lighter : public braft::StateMachine {
public:
    Lighter()
        : _node(NULL)
        , _value(0)
        , _leader_term(-1)
    {}

    ~Lighter() {
        delete _node;
    }

    // Starts this node
    int start() {
        butil::EndPoint addr(butil::my_ip(), FLAGS_port);
        braft::NodeOptions node_options;
        if (node_options.initial_conf.parse_from(FLAGS_conf) != 0) {
            LOG(ERROR) << "Fail to parse configuration `" << FLAGS_conf << '\'';
            return -1;
        }
        node_options.election_timeout_ms = FLAGS_election_timeout_ms;
        node_options.fsm = this;
        node_options.node_owns_fsm = false;
        node_options.snapshot_interval_s = FLAGS_snapshot_interval;
        std::string prefix = "local://" + FLAGS_data_path;
        node_options.log_uri = prefix + "/log";
        node_options.raft_meta_uri = prefix + "/raft_meta";
        node_options.snapshot_uri = prefix + "/snapshot";
        node_options.disable_cli = FLAGS_disable_cli;
        braft::Node* node = new braft::Node(FLAGS_group, braft::PeerId(addr));
        if (node->init(node_options) != 0) {
            LOG(ERROR) << "Fail to init raft node";
            delete node;
            return -1;
        }
        _node = node;
        return 0;
    }

    // Impelements Service methods
    void incrby(const IncrbyRequest* request,
                   LighterResponse* response,
                   google::protobuf::Closure* done) {
        brpc::ClosureGuard done_guard(done);
        // Serialize request to the replicated write-ahead-log so that all the
        // peers in the group receive this request as well.
        // Notice that _value can't be modified in this routine otherwise it
        // will be inconsistent with others in this group.
        
        // Serialize request to IOBuf
        const int64_t term = _leader_term.load(butil::memory_order_relaxed);
        if (term < 0) {
            return redirect(response);
        }
        butil::IOBuf log;
        butil::IOBufAsZeroCopyOutputStream wrapper(&log);
        if (!request->SerializeToZeroCopyStream(&wrapper)) {
            LOG(ERROR) << "Fail to serialize request";
            response->set_success(false);
            return;
        }
        // Apply this log as a braft::Task
        braft::Task task;
        task.data = &log;
        // This callback would be iovoked when the task actually excuted or fail
        task.done = new IncrbyClosure(this, request, response, done_guard.release());
        if (FLAGS_check_term) {
            // ABA problem can be avoid if expected_term is set
            task.expected_term = term;
        }
        // Now the task is applied to the group, waiting for the result.
        return _node->apply(task);
    }

    void get(LighterResponse* response) {
        // In consideration of consistency. GetRequest to follower should be rejected.
        if (!is_leader()) {
            // This node is a follower or not up-to-date. Redirect to leader if possible.
            return redirect(response);
        }

        // This is the leader and is up-to-date. It's safe to respond client
        response->set_success(true);
        response->set_value(_value.load(butil::memory_order_relaxed));
    }

    bool is_leader() const { 
        return _leader_term.load(butil::memory_order_acquire) > 0; 
    }

    // Shut this node down.
    void shutdown() {
        if (_node) {
            _node->shutdown(NULL);
        }
    }

    // Blocking this thread until the node is eventually down.
    void join() {
        if (_node) {
            _node->join();
        }
    }

private:
friend class IncrbyClosure;

    void redirect(LighterResponse* response) {
        response->set_success(false);
        if (_node) {
            braft::PeerId leader = _node->leader_id();
            if (!leader.is_empty()) {
                response->set_redirect(leader.to_string());
            }
        }
    }

    // @braft::StateMachine
    void on_apply(braft::Iterator& iter) {
        // A batch of tasks are committed, which must be processed through 
        // |iter|
        for (; iter.valid(); iter.next()) {
            int64_t detal_value = 0;
            LighterResponse* response = NULL;
            // This guard helps invoke iter.done()->Run() asynchronously to
            // avoid that callback blocks the StateMachine.
            braft::AsyncClosureGuard closure_guard(iter.done());
            if (iter.done()) {
                // This task is applied by this node, get value from this
                // closure to avoid additional parsing.
                IncrbyClosure* c = dynamic_cast<IncrbyClosure*>(iter.done());
                response = c->response();
                detal_value = c->request()->value();
            } else {
                // Have to parse IncrbyRequest from this log.
                butil::IOBufAsZeroCopyInputStream wrapper(iter.data());
                IncrbyRequest request;
                CHECK(request.ParseFromZeroCopyStream(&wrapper));
                detal_value = request.value();
            }

            // Now the log has been parsed. Update this state machine by this operation.
            const int64_t prev = _value.fetch_add(detal_value, butil::memory_order_relaxed);
            if (response) {
                response->set_success(true);
                response->set_value(prev);
            }

            // The purpose of following logs is to help you understand the way
            // this StateMachine works.
            // Remove these logs in performance-sensitive servers.
            LOG_IF(INFO, FLAGS_log_applied_task) 
                    << "Added value=" << prev << " by detal=" << detal_value
                    << " at log_index=" << iter.index();
        }
    }

    struct SnapshotArg {
        int64_t value;
        braft::SnapshotWriter* writer;
        braft::Closure* done;
    };

    static void *save_snapshot(void* arg) {
        SnapshotArg* sa = (SnapshotArg*) arg;
        std::unique_ptr<SnapshotArg> arg_guard(sa);
        // Serialize StateMachine to the snapshot
        brpc::ClosureGuard done_guard(sa->done);
        std::string snapshot_path = sa->writer->get_path() + "/data";
        LOG(INFO) << "Saving snapshot to " << snapshot_path;
        // Use protobuf to store the snapshot for backward compatibility.
        Snapshot s;
        s.set_value(sa->value);
        braft::ProtoBufFile pb_file(snapshot_path);
        if (pb_file.save(&s, true) != 0)  {
            sa->done->status().set_error(EIO, "Fail to save pb_file");
            return NULL;
        }
        // Snapshot is a set of files in raft. Add the only file into the writer here.
        if (sa->writer->add_file("data") != 0) {
            sa->done->status().set_error(EIO, "Fail to add file to writer");
            return NULL;
        }
        return NULL;
    }

    void on_snapshot_save(braft::SnapshotWriter* writer, braft::Closure* done) {
        // Save current StateMachine in memory and starts a new bthread to avoid
        // blocking StateMachine since it's a bit slow to write data to disk file.
        SnapshotArg* arg = new SnapshotArg;
        arg->value = _value.load(butil::memory_order_relaxed);
        arg->writer = writer;
        arg->done = done;
        bthread_t tid;
        bthread_start_urgent(&tid, NULL, save_snapshot, arg);
    }

    int on_snapshot_load(braft::SnapshotReader* reader) {
        // Load snasphot from reader, replacing the running StateMachine
        CHECK(!is_leader()) << "Leader is not supposed to load snapshot";
        if (reader->get_file_meta("data", NULL) != 0) {
            LOG(ERROR) << "Fail to find `data' on " << reader->get_path();
            return -1;
        }
        std::string snapshot_path = reader->get_path() + "/data";
        braft::ProtoBufFile pb_file(snapshot_path);
        Snapshot s;
        if (pb_file.load(&s) != 0) {
            LOG(ERROR) << "Fail to load snapshot from " << snapshot_path;
            return -1;
        }
        _value.store(s.value(), butil::memory_order_relaxed);
        return 0;
    }

    void on_leader_start(int64_t term) {
        _leader_term.store(term, butil::memory_order_release);
        LOG(INFO) << "Node becomes leader";
    }
    void on_leader_stop(const butil::Status& status) {
        _leader_term.store(-1, butil::memory_order_release);
        LOG(INFO) << "Node stepped down : " << status;
    }

    void on_shutdown() {
        LOG(INFO) << "This node is down";
    }
    void on_error(const ::braft::Error& e) {
        LOG(ERROR) << "Met raft error " << e;
    }
    void on_configuration_committed(const ::braft::Configuration& conf) {
        LOG(INFO) << "Configuration of this group is " << conf;
    }
    void on_stop_following(const ::braft::LeaderChangeContext& ctx) {
        LOG(INFO) << "Node stops following " << ctx;
    }
    void on_start_following(const ::braft::LeaderChangeContext& ctx) {
        LOG(INFO) << "Node start following " << ctx;
    }
    // end of @braft::StateMachine

private:
    braft::Node* volatile _node;
    butil::atomic<int64_t> _value;
    butil::atomic<int64_t> _leader_term;
};


// Implements grand::LighterService if you are using brpc.
class LighterServiceImpl : public LighterService {
public:
    explicit LighterServiceImpl(Lighter* lighter) : _lighter(lighter) {}

    void incrby(::google::protobuf::RpcController* controller,
                   const ::grand::IncrbyRequest* request,
                   ::grand::LighterResponse* response,
                   ::google::protobuf::Closure* done) {
        return _lighter->incrby(request, response, done);
    }

    void get(::google::protobuf::RpcController* controller,
             const ::grand::GetRequest* request,
             ::grand::LighterResponse* response,
             ::google::protobuf::Closure* done) {
        brpc::ClosureGuard done_guard(done);
        return _lighter->get(response);
    }

private:
    Lighter* _lighter;
};

void IncrbyClosure::Run() {
    // Auto delete this after Run()
    std::unique_ptr<IncrbyClosure> self_guard(this);
    // Repsond this RPC.
    brpc::ClosureGuard done_guard(_done);
    if (status().ok()) {
        return;
    }
    // Try redirect if this request failed.
    _lighter->redirect(_response);
}

}

int main(int argc, char* argv[]) {
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
    butil::AtExitManager exit_manager;

    // Generally you only need one Server.
    brpc::Server server;
    grand::Lighter lighter;
    grand::LighterServiceImpl service(&lighter);

    // Add your service into RPC server
    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }

    // raft can share the same RPC server. Notice the second parameter, because
    // adding services into a running server is not allowed and the listen
    // address of this server is impossible to get before the server starts. You
    // have to specify the address of the server.
    if (braft::add_service(&server, FLAGS_port) != 0) {
        LOG(ERROR) << "Fail to add raft service";
        return -1;
    }

    if (server.Start(FLAGS_port, NULL) != 0) {
        LOG(ERROR) << "Fail to start Server";
        return -1;
    }

    // It's ok to start Lighter;
    if (lighter.start() != 0) {
        LOG(ERROR) << "Fail to start Lighter";
        return -1;
    }

    LOG(INFO) << "Lighter service is running on " << server.listen_address();
    // Wait until 'CTRL-C' is pressed. then Stop() and Join() the service
    while (!brpc::IsAskedToQuit()) {
        sleep(1);
    }

    LOG(INFO) << "Lighter service is going to quit";

    // Stop lighter before server
    lighter.shutdown();
    server.Stop(0);

    // Wait until all the processing tasks are over.
    lighter.join();
    server.Join();
    return 0;
}
