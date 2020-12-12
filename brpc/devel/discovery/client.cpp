#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
#include "beta.pb.h"

DEFINE_bool(send_attachment, true, "Carry attachment along with requests");
DEFINE_string(protocol, "baidu_std", "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8003", "IP Address of server");
DEFINE_string(load_balancer, "", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)"); 

void HandleBetaResponse(brpc::Controller* cntl, discovery::BetaResponse* response) {
    std::unique_ptr<brpc::Controller> cntl_guard(cntl);
    std::unique_ptr<discovery::BetaResponse> response_guard(response);

    if (cntl->Failed()) {
        LOG(WARNING) << "Fail to send BetaRequest, " << cntl->ErrorText();
        return;
    }

    LOG(INFO) << "Received response from " << cntl->remote_side()
        << ": " << response->message() << " (attached="
        << cntl->response_attachment() << ")"
        << " latency=" << cntl->latency_us() << "us";
}
                        

int main(int argc, char* argv[]) {
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);

    brpc::ChannelOptions options;
    {
        options.protocol = FLAGS_protocol;
        options.connection_type = FLAGS_connection_type;
        options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
        options.max_retry = FLAGS_max_retry;
    }
    brpc::Channel channel;
    if (channel.Init(FLAGS_server.c_str(), FLAGS_load_balancer.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    discovery::BetaService_Stub stub(&channel);
    
    int log_id = 0;
    while (!brpc::IsAskedToQuit()) {
        discovery::BetaResponse* response = new discovery::BetaResponse();
        brpc::Controller* cntl = new brpc::Controller();

        discovery::BetaRequest request;
        request.set_message("beta message...");

        cntl->set_log_id(++log_id);  // set by user
        if (FLAGS_send_attachment) {
            cntl->request_attachment().append("I am beta's attachment.");
        }

        google::protobuf::Closure* done = brpc::NewCallback(&HandleBetaResponse, cntl, response);
        stub.Beta(cntl, &request, response, done);

        sleep(1);
    }

    LOG(INFO) << "BetaClient is going to quit";
    return 0;
}
