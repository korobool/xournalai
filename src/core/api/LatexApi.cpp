#include "LatexApi.h"

#include <atomic>    // for atomic
#include <fstream>   // for ifstream
#include <iterator>  // for istreambuf_iterator

#include <gio/gio.h>  // for GSubprocess

#include "control/Control.h"                 // for Control
#include "control/latex/LatexGenerator.h"    // for LatexGenerator
#include "control/settings/LatexSettings.h"  // for LatexSettings
#include "control/settings/Settings.h"       // for Settings
#include "model/TexImage.h"                  // for TexImage
#include "util/PathUtil.h"                   // for getTmpDirSubfolder

namespace xoj::api {

namespace {

/**
 * Fallback template for minimal TeX installations (no standalone.cls/scontents.sty): plain pdfLaTeX with
 * amsmath and xcolor, the page cropped to the formula with a 5pt border.
 */
constexpr const char* FALLBACK_TEMPLATE = R"TEX(\documentclass{article}
\usepackage{amsmath}
\usepackage{amssymb}
\usepackage{xcolor}
\definecolor{xpp_font_color}{HTML}{%%XPP_TEXT_COLOR%%}
\pagestyle{empty}
\begin{document}
\setbox0\hbox{\color{xpp_font_color}$\displaystyle %%XPP_TOOL_INPUT%%$}
\pdfpagewidth=\wd0 \advance\pdfpagewidth by 10pt
\pdfpageheight=\ht0 \advance\pdfpageheight by \dp0 \advance\pdfpageheight by 10pt
\hoffset=-1in \voffset=-1in
\shipout\vbox{\kern5pt\hbox{\kern5pt\box0\kern5pt}\kern5pt}
\end{document}
)TEX";

struct Job {
    Control* control;
    std::string latex;
    Color color;
    fs::path dir;
    std::function<void(LatexResult)> done;
    bool usedFallback = false;
};

void start(Job* job, const std::string& templateText);

std::string tail(const std::string& log, size_t lines) {
    size_t pos = log.size();
    for (size_t i = 0; i < lines && pos > 0; i++) {
        pos = log.rfind('\n', pos - 1);
        if (pos == std::string::npos) {
            return log;
        }
    }
    return log.substr(pos + 1);
}

void finish(Job* job, LatexResult r) {
    std::error_code ec;
    fs::remove_all(job->dir, ec);
    job->done(std::move(r));
    delete job;
}

void onComplete(GObject* procObj, GAsyncResult* res, gpointer data) {
    auto* job = static_cast<Job*>(data);
    GSubprocess* proc = G_SUBPROCESS(procObj);
    char* out = nullptr;
    GError* err = nullptr;
    const bool exited = g_subprocess_communicate_utf8_finish(proc, res, &out, nullptr, &err);
    std::string log = out ? out : "";
    g_free(out);
    const bool failed = err || !exited || g_subprocess_get_exit_status(proc) != 0;
    g_object_unref(proc);
    const bool missingFile =
            log.find(".cls' not found") != std::string::npos || log.find(".sty' not found") != std::string::npos;
    if (failed && missingFile && !job->usedFallback) {
        if (err) {
            g_error_free(err);
        }
        g_message("LaTeX template needs packages that are not installed; retrying with a minimal template");
        job->usedFallback = true;
        start(job, FALLBACK_TEMPLATE);
        return;
    }
    if (failed) {
        std::string msg = err ? err->message : "LaTeX reported an error";
        if (err) {
            g_error_free(err);
        }
        // Keep the interesting part of the log: lines starting with '!' and a few after them
        std::string excerpt;
        for (size_t p = log.find("\n!"); p != std::string::npos && excerpt.size() < 1500; p = log.find("\n!", p + 1)) {
            excerpt += log.substr(p + 1, std::min<size_t>(300, log.size() - p - 1)) + "\n";
        }
        finish(job, {nullptr, "LaTeX failed: " + msg + "\n" + (excerpt.empty() ? tail(log, 15) : excerpt)});
        return;
    }
    std::ifstream in(job->dir / "tex.pdf", std::ios::binary);
    std::string pdf((std::istreambuf_iterator<char>(in)), {});
    auto img = std::make_unique<TexImage>();
    GError* loadErr = nullptr;
    if (pdf.empty() || !img->loadData(std::move(pdf), &loadErr) || !img->getPdf()) {
        std::string msg = loadErr ? loadErr->message : "no PDF produced";
        if (loadErr) {
            g_error_free(loadErr);
        }
        finish(job, {nullptr, "Could not load the LaTeX output: " + msg});
        return;
    }
    img->setText(job->latex);
    finish(job, {std::move(img), {}});
}

void start(Job* job, const std::string& templateText) {
    std::error_code ec;
    fs::remove_all(job->dir, ec);
    fs::create_directories(job->dir, ec);
    LatexGenerator generator(job->control->getSettings()->latexSettings);
    auto result = generator.asyncRun(job->dir, LatexGenerator::templateSub(job->latex, templateText, job->color));
    if (auto* err = std::get_if<LatexGenerator::GenError>(&result)) {
        finish(job, {nullptr, "Could not start LaTeX: " + err->message});
        return;
    }
    g_subprocess_communicate_utf8_async(std::get<GSubprocess*>(result), nullptr, nullptr, onComplete, job);
}

}  // namespace

void typesetLatex(Control* control, const std::string& latex, Color color, std::function<void(LatexResult)> done) {
    const LatexSettings& settings = control->getSettings()->latexSettings;
    std::ifstream templ(settings.globalTemplatePath, std::ios::binary);
    std::string templateText;
    bool fallback = false;
    if (templ) {
        templateText.assign(std::istreambuf_iterator<char>(templ), {});
    } else {
        templateText = FALLBACK_TEMPLATE;
        fallback = true;
    }
    static std::atomic<unsigned> counter{0};
    auto* job =
            new Job{control,         latex,   color, Util::getTmpDirSubfolder("mcp-tex-" + std::to_string(++counter)),
                    std::move(done), fallback};
    start(job, templateText);
}

}  // namespace xoj::api
