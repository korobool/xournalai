#include "OffUi.h"

#include <exception>  // for exception
#include <optional>   // for optional
#include <stdexcept>  // for invalid_argument
#include <utility>    // for move

#include "control/Control.h"                // for Control
#include "control/jobs/Job.h"               // for Job
#include "control/jobs/XournalScheduler.h"  // for XournalScheduler
#include "util/StallWatch.h"                // for stall::Activity

namespace xoj::mcp {

namespace {
class OffUiJob final: public Job {
public:
    OffUiJob(std::function<ToolResult()> work, Responder respond): work(std::move(work)), respond(std::move(respond)) {}

    JobType getType() override { return JOB_TYPE_AGENT; }

protected:
    void run() override {
        xoj::util::stall::Activity activity("agent work off the UI thread");
        try {
            result = work();
        } catch (const ToolError& e) {
            result = ToolResult::error(e.what());
        } catch (const std::invalid_argument& e) {
            result = ToolResult::error(e.what());
        } catch (const std::exception& e) {
            result = ToolResult::error(std::string("Internal error: ") + e.what());
        }
        result->prepare();  // serialize here, not on the UI thread
        callAfterRun();
    }

    void afterRun() override {  // UI thread
        if (respond) {
            respond(std::move(*result));
        }
    }

    void onDelete() override {  // the scheduler dropped the job (shutdown)
        if (respond && !result) {
            respond(ToolResult::error("The app is shutting down"));
            respond = nullptr;
        }
    }

private:
    std::function<ToolResult()> work;
    Responder respond;
    std::optional<ToolResult> result;
};
}  // namespace

void runOffUi(Control* ctrl, std::function<ToolResult()> work, Responder respond) {
    auto* job = new OffUiJob(std::move(work), std::move(respond));
    ctrl->getScheduler()->addJob(job, JOB_PRIORITY_HIGH);
    job->unref();
}

}  // namespace xoj::mcp
