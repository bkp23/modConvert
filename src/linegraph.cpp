#include "linegraph.h"
#include "ui_linegraph.h"

#include <QPainter>


lineGraph::lineGraph(QWidget *parent) : QWidget(parent), ui(new Ui::lineGraph)
{
    ui->setupUi(this);

    max = 128;
    numpoints = 500;
    numaxes = 3;
    for(int a=0; a<numpoints; a++)
      for(int b=0; b<3; b++)
        axes[b].append(0);
}

lineGraph::~lineGraph()
{
    delete ui;
}


///////////////////////////////////////////////////////////

void lineGraph::addPoints(int x, int y, int z)
{
  axes[0].append(x);
  axes[0].removeFirst();
  axes[1].append(y);
  axes[1].removeFirst();
  axes[2].append(z);
  axes[2].removeFirst();
}


void lineGraph::setMax(int m)
{
  max = m;
}


void lineGraph::setNumAxes(int n)
{
  numaxes = n;
  if (numaxes > 3)
    numaxes = 3;
}


void lineGraph::setNumPoints(int n)
{
  while (numpoints != n)
  {
    if (numpoints < n)
    {
      for(int a=0; a<3; a++)
        axes[a].append(0);
      numpoints++;
    }
    else
    {
      for(int a=0; a<3; a++)
        axes[a].removeFirst();
      numpoints--;
    }
  }
}


void  lineGraph::paintEvent(QPaintEvent *event)
{
  int prev, whichpt, ptval=0;

  QPainter painter(this);

  //painter.setRenderHint(QPainter::Antialiasing);

  // Draw background (white with black border)
  painter.setBrush(QColor(255, 255, 255, 255)); // the "brush" fills in areas
  painter.drawRect(0, 0, width()-1, height()-1);

  // draw mid-point
  painter.setPen(Qt::DotLine);
  painter.drawLine(0,scaley(0), width()-1,scaley(0));

  // draw axes
  for (int idx=0; idx<numaxes; idx++)
  {
    switch (idx)
    {  // the "pen" is used for drawing lines
      case 0:  painter.setPen(QColor(255, 0,   0,   255));  break; // red
      case 1:  painter.setPen(QColor(0,   255, 0,   255));  break; // green
      case 2:  painter.setPen(QColor(0,   0,   255, 255));  break; // blue
    }

    prev = axes[idx].at(0);
    whichpt = 0;

    for(int x=1; x<width(); x++) // for each pixel
    {
      if (whichpt < axes[idx].size())
        ptval = axes[idx].at(whichpt);
      while (whichpt <= x*axes[idx].size()/width()) // search sub-window for local max/min
      {
        if (abs(axes[idx].at(whichpt)) > abs(ptval)) // find local max/min
          ptval = axes[idx].at(whichpt);
        whichpt++;
      }
      painter.drawLine(x-1, scaley(prev), x, scaley(ptval));
      prev = ptval;
    }
  }
}


int lineGraph::scaley(int y)
{
  return 1+(-1*y*(height()-2)/(2*max)) + ((height()-2)/2); // height-2 ensures that the wave doesn't touch the border
}

