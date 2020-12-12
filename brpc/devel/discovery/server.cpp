#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include "beta.pb.h"

DEFINE_bool(send_attachment, true, "Carry attachment along with response");
DEFINE_int32(port, 8003, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state (waiting for client to close connection before server stops)");

class BetaServicer : public discovery::BetaService {
public:
    BetaServicer() {};
    virtual ~BetaServicer() {};
    virtual void Beta(google::protobuf::RpcController* cntl_base,
                      const discovery::BetaRequest* request,
                      discovery::BetaResponse* response,
                      google::protobuf::Closure* done) {
        brpc::ClosureGuard done_guard(done);
        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);

        LOG(INFO) << "Received request[log_id=" << cntl->log_id() 
                  << "] from " << cntl->remote_side()
                  << ": " << request->message()
                  << " (attached=" << cntl->request_attachment() << ")";

        response->set_message(request->message());
        if (FLAGS_send_attachment) {
            cntl->response_attachment().append("bar");
        }
    }
};

int main(int argc, char* argv[]) {
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);

    brpc::Server server;
    BetaServicer betasrv;
    if (server.AddService(&betasrv, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }

    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start BetaServer";
        return -1;
    }

    server.RunUntilAskedToQuit();
    return 0;
}
