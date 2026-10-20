/*
 * Xournal++
 *
 * A job which saves a Document
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>
#include <string>  // for string

#include "model/ElementInsertionPosition.h"  // for InsertionOrder

#include "BlockingJob.h"  // for BlockingJob

class Control;
class Layer;
class UndoAction;


class SaveJob: public BlockingJob {
public:
    /// `inBackground`: an agent's save (see Control::saveInBackground)
    SaveJob(Control* control, std::function<void(bool)> = [](bool) {}, bool inBackground = false);

protected:
    ~SaveJob() override;

public:
    void run() override;

    bool save();

    static void updatePreview(Control* control);

protected:
    void afterRun() override;

private:
    std::string lastError;
    /// Called after saving, with boolean parameter true on success, false on failure (error)
    std::function<void(bool)> callback;
    /// The undo step the save began at: changes made while it runs stay unsaved
    UndoAction* savedAt = nullptr;
    /// In the background, the user's selection stays: copies of its elements are saved in their layer
    const Layer* selectionLayer = nullptr;
    InsertionOrder selectionCopies;
};
