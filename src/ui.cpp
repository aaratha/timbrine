#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <QColor>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSGVertexColorMaterial>

#include "analysis.hpp"
#include "audio.hpp"

UiController::UiController(AudioCore *audioCore, AnalysisCore *analysisCore,
                           QObject *parent)
    : QObject(parent), audioCore(audioCore), analysisCore(analysisCore) {
  emit binCountChanged();
  emit umapPointsChanged();
  emit voronoiEdgesChanged();
  emit voronoiCellsChanged();
}

void UiController::rectangleClicked(int index) {
  lastClickedIndex = index;
  std::cerr << "rectangle clicked: " << index << "\n";
  emit lastClickedChanged();

  if (audioCore) {
    audioCore->setBinIndex(static_cast<size_t>(index));
  }
}

int UiController::nearestUmapIndex(float normX, float normY) const {
  if (!analysisCore) {
    return -1;
  }

  const auto &xs = analysisCore->getBinTimbreX();
  const auto &ys = analysisCore->getBinTimbreY();
  const size_t count = std::min(xs.size(), ys.size());
  if (count == 0) {
    return -1;
  }

  float minX = xs[0];
  float maxX = xs[0];
  float minY = ys[0];
  float maxY = ys[0];
  for (size_t i = 1; i < count; ++i) {
    minX = std::min(minX, xs[i]);
    maxX = std::max(maxX, xs[i]);
    minY = std::min(minY, ys[i]);
    maxY = std::max(maxY, ys[i]);
  }

  const float rangeX = maxX - minX;
  const float rangeY = maxY - minY;

  float bestDist = std::numeric_limits<float>::max();
  int bestIndex = -1;
  for (size_t i = 0; i < count; ++i) {
    const float nx = rangeX > 0.0f ? (xs[i] - minX) / rangeX : 0.5f;
    const float ny = rangeY > 0.0f ? (ys[i] - minY) / rangeY : 0.5f;
    const float dx = normX - nx;
    const float dy = normY - ny;
    const float d2 = dx * dx + dy * dy;
    if (d2 < bestDist) {
      bestDist = d2;
      bestIndex = static_cast<int>(i);
    }
  }

  return bestIndex;
}

int UiController::cellIndexAt(float normX, float normY) const {
  if (!analysisCore) {
    return -1;
  }

  const auto &xs = analysisCore->getBinTimbreX();
  const auto &ys = analysisCore->getBinTimbreY();
  const size_t count = std::min(xs.size(), ys.size());
  if (count == 0) {
    return -1;
  }

  const auto &cellsBefore = analysisCore->getVoronoiCells();
  if (cellsBefore.empty() && count >= 2) {
    analysisCore->computeVoronoiEdges();
  }
  const auto &cells = analysisCore->getVoronoiCells();
  if (cells.empty()) {
    return nearestUmapIndex(normX, normY);
  }

  float minX = xs[0];
  float maxX = xs[0];
  float minY = ys[0];
  float maxY = ys[0];
  for (size_t i = 1; i < count; ++i) {
    minX = std::min(minX, xs[i]);
    maxX = std::max(maxX, xs[i]);
    minY = std::min(minY, ys[i]);
    maxY = std::max(maxY, ys[i]);
  }

  const float rangeX = maxX - minX;
  const float rangeY = maxY - minY;
  auto normalizeX = [&](float x) {
    return rangeX > 0.0f ? (x - minX) / rangeX : 0.5f;
  };
  auto normalizeY = [&](float y) {
    return rangeY > 0.0f ? (y - minY) / rangeY : 0.5f;
  };

  auto pointInPolygon = [&](const std::vector<float> &coords) {
    bool inside = false;
    const size_t n = coords.size() / 2;
    if (n < 3) {
      return false;
    }
    size_t j = n - 1;
    for (size_t i = 0; i < n; ++i) {
      const float xi = normalizeX(coords[i * 2]);
      const float yi = normalizeY(coords[i * 2 + 1]);
      const float xj = normalizeX(coords[j * 2]);
      const float yj = normalizeY(coords[j * 2 + 1]);
      const bool intersect =
          ((yi > normY) != (yj > normY)) &&
          (normX < (xj - xi) * (normY - yi) / (yj - yi + 1e-6f) + xi);
      if (intersect) {
        inside = !inside;
      }
      j = i;
    }
    return inside;
  };

  for (const auto &cell : cells) {
    if (pointInPolygon(cell.coords)) {
      return cell.site;
    }
  }

  return nearestUmapIndex(normX, normY);
}

int UiController::lastClicked() const { return lastClickedIndex; }

VoronoiEdgesItem::VoronoiEdgesItem(QQuickItem *parent)
    : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
}

QVariantList VoronoiEdgesItem::edges() const { return edgeList; }

void VoronoiEdgesItem::setEdges(const QVariantList &edges) {
  edgeList = edges;
  rebuildEdgeCache();
  geometryDirty = true;
  update();
  emit edgesChanged();
}

QVariantList VoronoiEdgesItem::cells() const { return cellList; }

void VoronoiEdgesItem::setCells(const QVariantList &cells) {
  cellList = cells;
  rebuildCellCache();
  geometryDirty = true;
  update();
  emit cellsChanged();
}

int VoronoiEdgesItem::activeIndex() const { return active; }

void VoronoiEdgesItem::setActiveIndex(int index) {
  if (active == index) {
    return;
  }
  active = index;
  colorDirty = true;
  update();
  emit activeIndexChanged();
}

void VoronoiEdgesItem::rebuildEdgeCache() {
  edgeCache.clear();
  edgeCache.reserve(static_cast<size_t>(edgeList.size()));
  for (const auto &entry : edgeList) {
    const auto map = entry.toMap();
    Edge edge;
    edge.x1 = map.value("x1").toFloat();
    edge.y1 = map.value("y1").toFloat();
    edge.x2 = map.value("x2").toFloat();
    edge.y2 = map.value("y2").toFloat();
    edge.site = map.value("site").toInt();
    edgeCache.push_back(edge);
  }
}

void VoronoiEdgesItem::rebuildCellCache() {
  cellCache.clear();
  cellCache.reserve(static_cast<size_t>(cellList.size()));
  for (const auto &entry : cellList) {
    const auto map = entry.toMap();
    Cell cell;
    cell.site = map.value("site").toInt();
    const auto coords = map.value("coords").toList();
    cell.points.reserve(static_cast<size_t>(coords.size() / 2));
    for (int i = 0; i + 1 < coords.size(); i += 2) {
      cell.points.emplace_back(coords[i].toFloat(), coords[i + 1].toFloat());
    }
    cellCache.push_back(std::move(cell));
  }
}

QSGNode *VoronoiEdgesItem::updatePaintNode(
    QSGNode *oldNode, UpdatePaintNodeData * /*updatePaintNodeData*/) {
  auto *root = static_cast<QSGNode *>(oldNode);
  if (!root) {
    root = new QSGNode();

    auto *fillNode = new QSGGeometryNode();
    auto *fillGeometry =
        new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
    fillGeometry->setDrawingMode(QSGGeometry::DrawTriangles);
    fillNode->setGeometry(fillGeometry);
    fillNode->setFlag(QSGNode::OwnsGeometry);
    auto *fillMaterial = new QSGVertexColorMaterial();
    fillMaterial->setFlag(QSGMaterial::Blending, true);
    fillNode->setMaterial(fillMaterial);
    fillNode->setFlag(QSGNode::OwnsMaterial);

    auto *lineNode = new QSGGeometryNode();
    auto *lineGeometry =
        new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
    lineGeometry->setDrawingMode(QSGGeometry::DrawLines);
    lineNode->setGeometry(lineGeometry);
    lineNode->setFlag(QSGNode::OwnsGeometry);
    lineNode->setMaterial(new QSGVertexColorMaterial());
    lineNode->setFlag(QSGNode::OwnsMaterial);

    root->appendChildNode(fillNode);
    root->appendChildNode(lineNode);
  }

  auto *fillNode = static_cast<QSGGeometryNode *>(root->firstChild());
  auto *lineNode = static_cast<QSGGeometryNode *>(fillNode->nextSibling());
  auto *lineGeometry = lineNode->geometry();
  auto *fillGeometry = fillNode->geometry();

  const int edgeCount = static_cast<int>(edgeCache.size());
  const int lineVertexCount = edgeCount * 2;
  if (geometryDirty || lineGeometry->vertexCount() != lineVertexCount) {
    lineGeometry->allocate(lineVertexCount);
    geometryDirty = true;
  }

  if (lineVertexCount == 0 || width() <= 0.0 || height() <= 0.0) {
    return root;
  }

  if (geometryDirty || colorDirty) {
    const float paddedWidth = std::max(0.0, width());
    const float paddedHeight = std::max(0.0, height());

    auto *lineVertices = lineGeometry->vertexDataAsColoredPoint2D();
    const QColor lineColor("#8a97a6");
    const unsigned char lineAlpha = 180;

    int v = 0;
    for (const auto &edge : edgeCache) {
      const float x1 = edge.x1 * paddedWidth;
      const float y1 = (1.0f - edge.y1) * paddedHeight;
      const float x2 = edge.x2 * paddedWidth;
      const float y2 = (1.0f - edge.y2) * paddedHeight;

      lineVertices[v++].set(x1, y1, lineColor.red(), lineColor.green(),
                            lineColor.blue(), lineAlpha);
      lineVertices[v++].set(x2, y2, lineColor.red(), lineColor.green(),
                            lineColor.blue(), lineAlpha);
    }

    lineGeometry->markVertexDataDirty();
    lineNode->markDirty(QSGNode::DirtyGeometry);
  }

  int fillVertexCount = 0;
  const Cell *activeCell = nullptr;
  if (active >= 0) {
    for (const auto &cell : cellCache) {
      if (cell.site == active && cell.points.size() >= 3) {
        activeCell = &cell;
        break;
      }
    }
  }
  if (activeCell) {
    fillVertexCount = static_cast<int>((activeCell->points.size() - 2) * 3);
  }

  if (geometryDirty || fillGeometry->vertexCount() != fillVertexCount) {
    fillGeometry->allocate(fillVertexCount);
    geometryDirty = true;
  }

  if (fillVertexCount > 0) {
    const float paddedWidth = std::max(0.0, width());
    const float paddedHeight = std::max(0.0, height());
    auto *fillVertices = fillGeometry->vertexDataAsColoredPoint2D();
    const QColor fillColor("#f39c12");
    const unsigned char fillAlpha = 80;
    const float alphaFactor = fillAlpha / 255.0f;
    const unsigned char fillR =
        static_cast<unsigned char>(fillColor.red() * alphaFactor);
    const unsigned char fillG =
        static_cast<unsigned char>(fillColor.green() * alphaFactor);
    const unsigned char fillB =
        static_cast<unsigned char>(fillColor.blue() * alphaFactor);

    const auto &points = activeCell->points;
    const QPointF origin = points[0];
    int fv = 0;
    for (size_t i = 1; i + 1 < points.size(); ++i) {
      const QPointF &p1 = points[i];
      const QPointF &p2 = points[i + 1];

      const float x0 = origin.x() * paddedWidth;
      const float y0 = (1.0f - origin.y()) * paddedHeight;
      const float x1 = p1.x() * paddedWidth;
      const float y1 = (1.0f - p1.y()) * paddedHeight;
      const float x2 = p2.x() * paddedWidth;
      const float y2 = (1.0f - p2.y()) * paddedHeight;

      fillVertices[fv++].set(x0, y0, fillR, fillG, fillB, fillAlpha);
      fillVertices[fv++].set(x1, y1, fillR, fillG, fillB, fillAlpha);
      fillVertices[fv++].set(x2, y2, fillR, fillG, fillB, fillAlpha);
    }

    fillGeometry->markVertexDataDirty();
    fillNode->markDirty(QSGNode::DirtyGeometry);
  }

  geometryDirty = false;
  colorDirty = false;

  return root;
}

void VoronoiEdgesItem::geometryChange(const QRectF &newGeometry,
                                      const QRectF &oldGeometry) {
  if (newGeometry.size() != oldGeometry.size()) {
    geometryDirty = true;
    update();
  }
  QQuickItem::geometryChange(newGeometry, oldGeometry);
}

int UiController::binCount() const {
  if (!analysisCore) {
    return 0;
  }
  return static_cast<int>(analysisCore->getBinCount());
}

QVariantList UiController::umapPoints() const {
  QVariantList points;
  if (!analysisCore) {
    return points;
  }

  const auto &xs = analysisCore->getBinTimbreX();
  const auto &ys = analysisCore->getBinTimbreY();
  const size_t count = std::min(xs.size(), ys.size());
  if (count == 0) {
    return points;
  }

  float minX = xs[0];
  float maxX = xs[0];
  float minY = ys[0];
  float maxY = ys[0];
  for (size_t i = 1; i < count; ++i) {
    minX = std::min(minX, xs[i]);
    maxX = std::max(maxX, xs[i]);
    minY = std::min(minY, ys[i]);
    maxY = std::max(maxY, ys[i]);
  }

  const float rangeX = maxX - minX;
  const float rangeY = maxY - minY;

  points.reserve(static_cast<int>(count));
  for (size_t i = 0; i < count; ++i) {
    const float normX = rangeX > 0.0f ? (xs[i] - minX) / rangeX : 0.5f;
    const float normY = rangeY > 0.0f ? (ys[i] - minY) / rangeY : 0.5f;
    QVariantMap point;
    point.insert("x", normX);
    point.insert("y", normY);
    point.insert("index", static_cast<int>(i));
    points.append(point);
  }

  return points;
}

QVariantList UiController::voronoiEdges() const {
  QVariantList edges;
  if (!analysisCore) {
    return edges;
  }

  const auto &xs = analysisCore->getBinTimbreX();
  const auto &ys = analysisCore->getBinTimbreY();
  const size_t count = std::min(xs.size(), ys.size());
  if (count == 0) {
    return edges;
  }

  const auto &edgesBefore = analysisCore->getVoronoiEdges();
  if (edgesBefore.empty() && count >= 2) {
    analysisCore->computeVoronoiEdges();
  }
  const auto &edgeList = analysisCore->getVoronoiEdges();
  if (edgeList.empty()) {
    return edges;
  }

  float minX = xs[0];
  float maxX = xs[0];
  float minY = ys[0];
  float maxY = ys[0];
  for (size_t i = 1; i < count; ++i) {
    minX = std::min(minX, xs[i]);
    maxX = std::max(maxX, xs[i]);
    minY = std::min(minY, ys[i]);
    maxY = std::max(maxY, ys[i]);
  }

  const float rangeX = maxX - minX;
  const float rangeY = maxY - minY;

  auto normalizeX = [&](float x) {
    return rangeX > 0.0f ? (x - minX) / rangeX : 0.5f;
  };
  auto normalizeY = [&](float y) {
    return rangeY > 0.0f ? (y - minY) / rangeY : 0.5f;
  };

  edges.reserve(static_cast<int>(edgeList.size()));
  for (const auto &edge : edgeList) {
    QVariantMap edgeMap;
    edgeMap.insert("x1", normalizeX(edge.x1));
    edgeMap.insert("y1", normalizeY(edge.y1));
    edgeMap.insert("x2", normalizeX(edge.x2));
    edgeMap.insert("y2", normalizeY(edge.y2));
    edgeMap.insert("site", edge.site);
    edges.append(edgeMap);
  }

  return edges;
}

QVariantList UiController::voronoiCells() const {
  QVariantList cells;
  if (!analysisCore) {
    return cells;
  }

  const auto &xs = analysisCore->getBinTimbreX();
  const auto &ys = analysisCore->getBinTimbreY();
  const size_t count = std::min(xs.size(), ys.size());
  if (count == 0) {
    return cells;
  }

  const auto &cellsBefore = analysisCore->getVoronoiCells();
  if (cellsBefore.empty() && count >= 2) {
    analysisCore->computeVoronoiEdges();
  }
  const auto &cellList = analysisCore->getVoronoiCells();
  if (cellList.empty()) {
    return cells;
  }

  float minX = xs[0];
  float maxX = xs[0];
  float minY = ys[0];
  float maxY = ys[0];
  for (size_t i = 1; i < count; ++i) {
    minX = std::min(minX, xs[i]);
    maxX = std::max(maxX, xs[i]);
    minY = std::min(minY, ys[i]);
    maxY = std::max(maxY, ys[i]);
  }

  const float rangeX = maxX - minX;
  const float rangeY = maxY - minY;

  auto normalizeX = [&](float x) {
    return rangeX > 0.0f ? (x - minX) / rangeX : 0.5f;
  };
  auto normalizeY = [&](float y) {
    return rangeY > 0.0f ? (y - minY) / rangeY : 0.5f;
  };

  cells.reserve(static_cast<int>(cellList.size()));
  for (const auto &cell : cellList) {
    QVariantList coords;
    coords.reserve(static_cast<int>(cell.coords.size()));
    for (size_t i = 0; i + 1 < cell.coords.size(); i += 2) {
      coords.append(normalizeX(cell.coords[i]));
      coords.append(normalizeY(cell.coords[i + 1]));
    }

    QVariantMap cellMap;
    cellMap.insert("site", cell.site);
    cellMap.insert("coords", coords);
    cells.append(cellMap);
  }

  return cells;
}
