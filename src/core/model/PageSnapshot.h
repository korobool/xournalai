/*
 * xournalai (based on Xournal++)
 *
 * A copy of a page to render or read without holding the document lock. Take it under a (short) shared lock, then
 * work on the copy with no lock: the UI thread, which needs the exclusive lock to add the user's stroke, is then
 * blocked only for the copy, not for the whole render or serialization.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr

#include "model/PageRef.h"  // for PageRef
#include "util/Range.h"     // for Range

class XojPage;

namespace xoj::model {

/**
 * Backgrounds, layers (names, visibility) and elements of `page`; with `only`, just the elements intersecting it.
 * Returns nullptr if a stroke in scope is being erased right now (that live state is not copied): then work on the
 * page itself, under the lock.
 */
PageRef snapshotPage(const XojPage& page, const Range* only = nullptr);

}  // namespace xoj::model
