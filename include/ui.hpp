#pragma once

#include <QObject>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QVariant>
#include <QtQml/qqmlregistration.h>
#include <vector>

class AnalysisCore;
class AudioCore;

class UiController : public QObject {
  Q_OBJECT
  Q_PROPERTY(int lastClicked READ lastClicked NOTIFY lastClickedChanged)
  Q_PROPERTY(int binCount READ binCount NOTIFY binCountChanged)
  Q_PROPERTY(QVariantList umapPoints READ umapPoints NOTIFY umapPointsChanged)
  Q_PROPERTY(QVariantList voronoiEdges READ voronoiEdges NOTIFY
                 voronoiEdgesChanged)
  Q_PROPERTY(QVariantList voronoiCells READ voronoiCells NOTIFY
                 voronoiCellsChanged)

public:
  explicit UiController(AudioCore *audioCore, AnalysisCore *analysisCore,
                        QObject *parent = nullptr);

  Q_INVOKABLE void rectangleClicked(int index);
  Q_INVOKABLE int nearestUmapIndex(float normX, float normY) const;
  Q_INVOKABLE int cellIndexAt(float normX, float normY) const;
  int lastClicked() const;
  int binCount() const;
  QVariantList umapPoints() const;
  QVariantList voronoiEdges() const;
  QVariantList voronoiCells() const;

signals:
  void lastClickedChanged();
  void binCountChanged();
  void umapPointsChanged();
  void voronoiEdgesChanged();
  void voronoiCellsChanged();

private:
  AudioCore *audioCore;
  AnalysisCore *analysisCore;
  int lastClickedIndex{-1};
};

class VoronoiEdgesItem : public QQuickItem {
  Q_OBJECT
  QML_NAMED_ELEMENT(VoronoiEdges)
  QML_ADDED_IN_VERSION(1, 0)
  Q_PROPERTY(QVariantList edges READ edges WRITE setEdges NOTIFY edgesChanged)
  Q_PROPERTY(QVariantList cells READ cells WRITE setCells NOTIFY cellsChanged)
  Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY
                 activeIndexChanged)

public:
  explicit VoronoiEdgesItem(QQuickItem *parent = nullptr);

  QVariantList edges() const;
  void setEdges(const QVariantList &edges);

  QVariantList cells() const;
  void setCells(const QVariantList &cells);

  int activeIndex() const;
  void setActiveIndex(int index);


signals:
  void edgesChanged();
  void cellsChanged();
  void activeIndexChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *oldNode,
                           UpdatePaintNodeData *updatePaintNodeData) override;
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;

private:
  struct Edge {
    float x1;
    float y1;
    float x2;
    float y2;
    int site;
  };

  struct Cell {
    int site;
    std::vector<QPointF> points;
  };

  void rebuildEdgeCache();
  void rebuildCellCache();

  QVariantList edgeList;
  QVariantList cellList;
  std::vector<Edge> edgeCache;
  std::vector<Cell> cellCache;
  int active{ -1 };
  bool geometryDirty{ true };
  bool colorDirty{ true };
};
