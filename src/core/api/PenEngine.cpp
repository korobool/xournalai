#include "PenEngine.h"

#include <algorithm>      // for max
#include <cmath>          // for hypot
#include <memory>         // for unique_ptr
#include <unordered_set>  // for unordered_set

#include <glib.h>
#include <gtk/gtk.h>

#include "control/Control.h"                // for Control
#include "control/Tool.h"                   // for Tool
#include "control/ToolHandler.h"            // for ToolHandler
#include "control/layer/LayerController.h"  // for LayerController
#include "control/tools/EditSelection.h"    // for EditSelection
#include "control/zoom/ZoomControl.h"       // for ZoomControl
#include "gui/MainWindow.h"                 // for MainWindow
#include "gui/PageView.h"                   // for XojPageView
#include "gui/XournalView.h"                // for XournalView
#include "gui/inputdevices/InputContext.h"  // for InputContext
#include "gui/inputdevices/InputEvents.h"   // for InputEvent
#include "gui/widgets/XournalWidget.h"      // for GtkXournal
#include "model/Document.h"                 // for Document
#include "model/Layer.h"                    // for Layer
#include "model/XojPage.h"                  // for XojPage
#include "undo/UndoRedoHandler.h"           // for UndoRedoHandler

#include "Drafts.h"   // for isDraftLayer
#include "DrawApi.h"  // for resolveLayer

namespace xoj::api {

namespace {

constexpr guint TICK_MS = 8;
constexpr gint64 MAX_WAIT_FOR_USER_US = 30'000'000;

struct Event {
    InputEventType type;
    double x, y, pressure;
    double at;  ///< ms from the start of the job
};

struct Run {
    Control* control;
    PenJob job;
    std::function<void(PenResult)> done;
    std::vector<Event> events;
    size_t next = 0;
    gint64 startUs = 0;
    gint64 waitStartUs = 0;
    bool started = false;
    // saved user state
    ToolType savedTool = TOOL_PEN;
    Color savedColor{};
    ToolSize savedSize = TOOL_SIZE_MEDIUM;
    DrawingType savedDrawingType = DRAWING_TYPE_DEFAULT;
    size_t savedLayer = 0;
    PageRef page;
    std::unordered_set<const Element*> before;
    PenResult result;
};

std::unordered_set<const Element*> elementsOf(const PageRef& page) {
    std::unordered_set<const Element*> out;
    for (const Layer* l: page->getLayersView()) {
        for (const Element* e: l->getElementsView()) {
            out.insert(e);
        }
    }
    return out;
}

InputEvent makeEvent(Control* ctrl, size_t pageIndex, const Event& ev) {
    XournalView* xv = ctrl->getWindow()->getXournal();
    GtkXournal* xw = GTK_XOURNAL(xv->getWidget());
    XojPageView* view = xv->getViewFor(pageIndex);
    const auto pos = view->getPixelPosition();
    const double zoom = ctrl->getZoomControl()->getZoom();
    GdkDevice* dev = gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default()));
    InputEvent e;
    e.device = dev;
    e.type = ev.type;
    e.deviceClass = INPUT_DEVICE_PEN;
    e.deviceName = "xournalai-agent-pen";
    e.relative = {pos.x + ev.x * zoom, pos.y + ev.y * zoom};
    e.absolute = {e.relative.x - gtk_adjustment_get_value(xw->hadjustment),
                  e.relative.y - gtk_adjustment_get_value(xw->vadjustment)};
    e.button = 1;
    e.state = ev.type == BUTTON_PRESS_EVENT ? GdkModifierType(0) : GDK_BUTTON1_MASK;
    e.pressure = ev.pressure;
    e.timestamp = static_cast<guint32>(ev.at);
    e.deviceId = DeviceId(dev);
    return e;
}

/// Puts the settings of the tool the pen used back to what the user had
void restoreToolSettings(Run* r) {
    ToolHandler* th = r->control->getToolHandler();
    if (th->getToolType() != r->job.tool) {
        r->control->selectTool(r->job.tool);
    }
    if (r->job.color) {
        th->setColor(r->savedColor, false);
    }
    if (r->job.size) {
        th->setSize(r->savedSize);
    }
    if (r->job.drawingType) {
        th->setDrawingType(r->savedDrawingType);
    }
}

void restore(Run* r) {
    restoreToolSettings(r);
    r->control->selectTool(r->savedTool);
    if (r->savedLayer > 0 && r->page->getSelectedLayerId() != r->savedLayer) {
        r->page->setSelectedLayerId(r->savedLayer);
        if (r->control->getCurrentPage() == r->page) {
            r->control->getLayerController()->switchToLay(r->savedLayer, false, false);
        }
    }
}

void finish(Run* r) {
    // A selection made by the pen stays for the agent to act on; everything else is restored
    if (auto* sel = r->control->getWindow()->getXournal()->getSelection()) {
        r->result.selected = sel->getElementsView().size();
    }
    const auto after = elementsOf(r->page);
    for (const Element* e: after) {
        if (!r->before.count(e)) {
            r->result.created.push_back(e);
        }
    }
    for (const Element* e: r->before) {
        if (!after.count(e)) {
            r->result.erased++;
        }
    }
    const bool keepSelectionTool = r->result.selected > 0;
    if (!keepSelectionTool) {
        restore(r);
    } else {
        // Keep the selection tool active so the selection survives; restore tool settings only
        restoreToolSettings(r);
    }
    r->done(std::move(r->result));
    delete r;
}

bool begin(Run* r) {
    Control* ctrl = r->control;
    ToolHandler* th = ctrl->getToolHandler();
    r->savedTool = th->getToolType();
    r->savedColor = th->getTool(r->job.tool).getColor();
    r->savedSize = th->getTool(r->job.tool).getSize();
    r->savedDrawingType = th->getTool(r->job.tool).getDrawingType();
    r->savedLayer = r->page->getSelectedLayerId();

    // Target layer: selecting it makes the tools act there
    DrawApi draw(ctrl);
    LayerChoice layer = draw.resolveLayer(r->page, r->job.layer, true);
    if (Drafts::get().isDraftLayer(layer.layer)) {
        throw std::invalid_argument("The pen engine cannot draw into drafts (it records undo steps like the user); "
                                    "use create_strokes/create_shapes/create_from_svg for drafts");
    }
    if (layer.undo) {
        ctrl->getUndoRedoHandler()->addUndoAction(std::move(layer.undo));
    }
    r->result.layer = layer.index;
    r->result.layerName = layer.name;
    if (r->page->getSelectedLayerId() != layer.index) {
        r->page->setSelectedLayerId(layer.index);
    }

    ctrl->selectTool(r->job.tool);
    if (r->job.color) {
        th->setColor(*r->job.color, false);
    }
    if (r->job.size) {
        th->setSize(*r->job.size);
    }
    if (r->job.drawingType) {
        th->setDrawingType(*r->job.drawingType);
    }
    r->before = elementsOf(r->page);
    r->startUs = g_get_monotonic_time();
    r->started = true;
    return true;
}

gboolean tick(gpointer data) {
    auto* r = static_cast<Run*>(data);
    Control* ctrl = r->control;
    if (!r->started) {
        if (userInputActive(ctrl)) {  // the user is drawing: wait
            if (g_get_monotonic_time() - r->waitStartUs > MAX_WAIT_FOR_USER_US) {
                r->result.error = "The user kept drawing for 30 s; try again later";
                r->done(std::move(r->result));
                delete r;
                return G_SOURCE_REMOVE;
            }
            return G_SOURCE_CONTINUE;
        }
        try {
            begin(r);
        } catch (const std::exception& e) {  // e.g. an invalid layer: nothing was changed yet
            r->result.error = e.what();
            r->done(std::move(r->result));
            delete r;
            return G_SOURCE_REMOVE;
        }
    }
    InputContext* ic = GTK_XOURNAL(ctrl->getWindow()->getXournal()->getWidget())->input;
    const double elapsed = r->job.speed <= 0 ? 1e18 : static_cast<double>(g_get_monotonic_time() - r->startUs) / 1000.0;
    if (r->job.page >= ctrl->getWindow()->getXournal()->getViewPages().size() ||
        ctrl->getWindow()->getXournal()->getViewFor(r->job.page) == nullptr) {  // document changed meanwhile
        r->result.error = "The document changed while the pen was drawing; stopped";
        restore(r);
        r->done(std::move(r->result));
        delete r;
        return G_SOURCE_REMOVE;
    }
    while (r->next < r->events.size() && r->events[r->next].at <= elapsed) {
        ic->handleSynthetic(makeEvent(ctrl, r->job.page, r->events[r->next]));
        r->next++;
    }
    if (r->next < r->events.size()) {
        return G_SOURCE_CONTINUE;
    }
    finish(r);
    return G_SOURCE_REMOVE;
}

}  // namespace

bool userInputActive(Control* control) {
    if (!control->getWindow()) {
        return false;
    }
    for (const auto& view: control->getWindow()->getXournal()->getViewPages()) {
        if (view->hasActiveInput()) {
            return true;
        }
    }
    return false;
}

void runPen(Control* control, PenJob job, std::function<void(PenResult)> done) {
    auto* r = new Run();
    r->control = control;
    r->done = std::move(done);
    {
        std::shared_lock lock(*control->getDocument());
        r->page = control->getDocument()->getPage(job.page);
    }
    // Timeline: each stroke = press, moves, release; a short pen-up pause between strokes
    const double speed = job.speed <= 0 ? 1.0 : job.speed;
    double clock = 0;
    for (const auto& stroke: job.strokes) {
        if (stroke.empty()) {
            continue;
        }
        const double strokeStart = clock;
        for (size_t i = 0; i < stroke.size(); i++) {
            const PenSample& s = stroke[i];
            double at;
            if (s.t >= 0) {
                at = strokeStart + s.t / speed;
            } else if (i == 0) {
                at = strokeStart;
            } else {
                const double d = std::hypot(s.x - stroke[i - 1].x, s.y - stroke[i - 1].y);
                at = r->events.back().at + 1000.0 * d / (job.pointsPerSecond * speed);
            }
            r->events.push_back({i == 0 ? BUTTON_PRESS_EVENT : MOTION_EVENT, s.x, s.y, s.pressure, at});
        }
        const PenSample& last = stroke.back();
        r->events.push_back({BUTTON_RELEASE_EVENT, last.x, last.y, last.pressure, r->events.back().at + 8});
        clock = r->events.back().at + 60.0 / speed;
    }
    r->job = std::move(job);
    r->waitStartUs = g_get_monotonic_time();
    if (r->job.speed <= 0 && !userInputActive(control)) {
        tick(r);  // instant: replay synchronously
        return;
    }
    g_timeout_add(TICK_MS, tick, r);
}

}  // namespace xoj::api
