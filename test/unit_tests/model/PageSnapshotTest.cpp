#include <gtest/gtest.h>

#include "model/Layer.h"
#include "model/PageSnapshot.h"
#include "model/Stroke.h"
#include "model/XojPage.h"

namespace {
auto stroke(double x, double y) -> ElementPtr {
    auto s = std::make_unique<Stroke>();
    s->addPoint(Point(x, y));
    s->addPoint(Point(x + 10, y + 10));
    return s;
}
}  // namespace

TEST(PageSnapshot, KeepsTheBackgroundOfAnOrdinaryPage) {
    // A snapshot of a lined page once became a PDF page ("PDF background missing" on every page)
    XojPage page(595, 842);
    page.setBackgroundType(PageType(PageTypeFormat::Lined));
    page.setBackgroundColor(Color(0xffeeddu));
    auto snap = xoj::model::snapshotPage(page);
    ASSERT_TRUE(snap);
    EXPECT_FALSE(snap->getBackgroundType().isPdfPage());
    EXPECT_EQ(snap->getBackgroundType().format, PageTypeFormat::Lined);
    EXPECT_EQ(snap->getBackgroundColor(), Color(0xffeeddu));
    EXPECT_EQ(snap->getWidth(), 595);
    EXPECT_EQ(snap->getHeight(), 842);
}

TEST(PageSnapshot, KeepsAPdfBackground) {
    XojPage page(595, 842);
    page.setBackgroundPdfPageNr(3);
    auto snap = xoj::model::snapshotPage(page);
    ASSERT_TRUE(snap);
    EXPECT_TRUE(snap->getBackgroundType().isPdfPage());
    EXPECT_EQ(snap->getPdfPageNr(), 3u);
}

TEST(PageSnapshot, CopiesLayersAndOnlyTheElementsAsked) {
    XojPage page(595, 842);  // one layer
    Layer* layer = page.getSelectedLayer();
    layer->addElement(stroke(10, 10));
    layer->addElement(stroke(300, 300));
    layer->setVisible(false);

    auto all = xoj::model::snapshotPage(page);
    ASSERT_EQ(all->getLayerCount(), 1u);
    EXPECT_EQ(all->getLayersView()[0]->getElementsView().size(), 2u);
    EXPECT_FALSE(all->getLayersView()[0]->isVisible());

    const Range area(0, 0, 50, 50);
    auto part = xoj::model::snapshotPage(page, &area);
    EXPECT_EQ(part->getLayersView()[0]->getElementsView().size(), 1u);
}
