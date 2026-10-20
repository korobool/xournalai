/*
 * Xournal++
 *
 * A job which is done in the GTK main thread, but the application is blocked
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include <glib-object.h>  // for GWeakRef

#include "Job.h"  // for Job, JobType

class Control;

class BlockingJob: public Job {
public:
    /// `blockUi` false: runs like the others, with the window usable (an agent's save)
    BlockingJob(Control* control, const std::string& name, bool blockUi = true);

protected:
    ~BlockingJob() override;

public:
    void execute() override;

    JobType getType() override;

protected:
    Control* control;
    const bool blocksUi;

private:
    /// The text input the user typed in when the job began (the AI terminal, an AI input field): blocking makes the
    /// canvas insensitive, and an input field shown over it (the AI Lasso's) with it, which loses the keyboard
    GWeakRef keyboardOwner;
};
