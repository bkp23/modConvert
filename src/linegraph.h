#ifndef LINEGRAPH_H
#define LINEGRAPH_H

#include <QWidget>

namespace Ui {
    class lineGraph;
}

class lineGraph : public QWidget
{
    Q_OBJECT

  public:
    explicit lineGraph(QWidget *parent = 0);
    ~lineGraph();
    void addPoints(int x, int y=0, int z=0);
    void setMax(int m);
    void setNumAxes(int n);
    void setNumPoints(int n);

  protected:
    void paintEvent(QPaintEvent *event);

  private:
    Ui::lineGraph *ui;
    int scaley(int y);

    QList<int> axes[3];
    int max, numpoints, numaxes;

};

#endif // LINEGRAPH_H
