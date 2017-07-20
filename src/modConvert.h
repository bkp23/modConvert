#ifndef MODCONVERT_H
#define MODCONVERT_H

#include <QMainWindow>
#include <QTime>
#include <QSound>
#include "modparser.h"

namespace Ui {
  class modConvert;
}

class modConvert : public QMainWindow
{
  Q_OBJECT

  public:
    explicit modConvert(QWidget *parent = 0);
    ~modConvert();

  private:
    Ui::modConvert *ui;
    void updateGraph();
    void writeWav(QByteArray arr, int repeat=0, int vol=64);

    //QTimer updateTimer, sampleTimer;
    QTime contTimer; // continuously-running timer
    QSound *qs;
    modParser *parser;
    int parserActive, sampleIndex[MAX_SAMPLES];


  public slots:
    void openFile();
    void saveFile();
    void radioChange();
    void sampleChange(int idx);
    void newTypeChange(int idx);
    void volumeChange(int idx);
    void diminishChange(int idx);
    void offsetChange(int idx);
    void playOrig();
    void playSynth();
    void playSong();
    void stopSong();
};

#endif // modConvert_H
