#include "modConvert.h"
#include "ui_modConvert.h"
#include "modparser.h"
#include <math.h> // cos()
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTimer> // Do I need this?

#define FILENAME    "miniMod.tmp.wav"
#define SAMPLERATE  8287 // used in recording .WAV and graphing waves
#define TICKSPERSEC 50 // only used in graphing waves

modConvert::modConvert(QWidget *parent) : QMainWindow(parent), ui(new Ui::modConvert)
{
  ui->setupUi(this);
  qs = new QSound(FILENAME);
  //sampleTimer.setSingleShot(false);
  //sampleTimer.start(500); // update twice a second
  //connect(&sampleTimer,      SIGNAL(timeout()), this, SLOT(sampleRate())    );
  connect(ui->stopSongButton, SIGNAL(clicked()),   this, SLOT(stopSong())    );
  connect(ui->songPlayButton, SIGNAL(clicked()),   this, SLOT(playSong())    );
  connect(ui->origPlayButton, SIGNAL(clicked()),   this, SLOT(playOrig())    );
  connect(ui->miniPlayButton, SIGNAL(clicked()),   this, SLOT(playSynth())   );
  connect(ui->actionOpen,     SIGNAL(triggered()), this, SLOT(openFile())    );
  connect(ui->actionSave,     SIGNAL(triggered()), this, SLOT(saveFile())    );
  connect(ui->sampleSelect,   SIGNAL(currentIndexChanged(int)), this, SLOT(sampleChange(int)) );
  connect(ui->newTypeBox,     SIGNAL(valueChanged(int)), this, SLOT(newTypeChange(int))  );
  connect(ui->volumeBox,      SIGNAL(valueChanged(int)), this, SLOT(volumeChange(int))   );
  connect(ui->diminishBox,    SIGNAL(valueChanged(int)), this, SLOT(diminishChange(int)) );
  connect(ui->offsetBox,      SIGNAL(valueChanged(int)), this, SLOT(offsetChange(int))   );
  // Radio buttons
  connect(ui->triButton,      SIGNAL(clicked()),   this, SLOT(radioChange()) );
  connect(ui->sawButton,      SIGNAL(clicked()),   this, SLOT(radioChange()) );
  connect(ui->squareButton,   SIGNAL(clicked()),   this, SLOT(radioChange()) );
  //connect(ui->sineButton,     SIGNAL(clicked()),   this, SLOT(radioChange()) );
  connect(ui->noiseButton,    SIGNAL(clicked()),   this, SLOT(radioChange()) );
  // Put a generic pattern on the graph
  ui->graph->setMax((1<<7)*64);
  ui->graph->setNumPoints(500);
  ui->graph->setNumAxes(2);
  for(int a=0; a<500; a++)
    ui->graph->addPoints(64*128*sin(2*3.1415*a/500),-32*128*sin(2*3.1415*a/500));

  ui->actionSave->setEnabled(false); // disable Save option until something is loaded
  parserActive = 0;
}

modConvert::~modConvert()
{
  QFile qf(FILENAME);
  if (qf.exists())
    qf.remove();
  delete qs;
  delete ui;
  if (parserActive)
    delete parser;
}

//////////////////////////////////////////////////////////////////////////////
// PRIVATE
void modConvert::updateGraph()
{
  const int SCALE = 50;
  int a; // looping var
  int size, repeat; // for displaying original data
  float data;
  float gendata=0, period=64.0; // for generated data
  int genvol = (ui->volumeBox->value() << 8);
  int gendiminish = ui->diminishBox->value();
  int speriod, x = sampleIndex[ui->sampleSelect->currentIndex()];
  if (!parserActive)
    return;
  int vol = parser->samples[x].volume;

  if (parser->samples[x].repeatLen > 0)
  {
    repeat = 1;
    size = parser->samples[x].repeatLen;
  }
  else
  {
    repeat = 0;
    size = parser->samples[x].length;
  }
  ui->graph->setNumPoints((size-1)*SCALE);
  for (a=0; a < ui->offsetBox->text().toInt(); a++)
    period /= 1.05946309;
  for (a=0; a > ui->offsetBox->text().toInt(); a--)
    period *= 1.05946309;
  speriod = period*SCALE;
  for (a=0; a<(size-1)*SCALE; a++)
  {
    if (ui->triButton->isChecked()) // Triangle wave
    {
      if ((a + (int)(speriod/4))%(int)speriod <= speriod/2)
        gendata = 256*((a + (int)(speriod/4))%(int)speriod) / (speriod/2) - 128;
      else
        gendata = 128-256*((a - (int)(speriod/4))%(int)speriod) / (speriod/2);
    }
    if (ui->sawButton->isChecked()) // Sawtooth wave
      gendata = 256*(speriod - (a+3*speriod/4)%(int)speriod)/speriod - 128;
    if (ui->squareButton->isChecked()) // Square wave
    {
      if (((a+speriod/4)%(int)speriod) < (speriod/2))
        gendata = -128;
      else
        gendata = 128;
    }
    //if (ui->sineButton->isChecked()) // Sine wave
    //  gendata = sin(a*2*3.14159265/speriod)*128;
    if (ui->noiseButton->isChecked()) // White noise
      gendata = (qrand()%256 - 128);
    if (a!=0 && a%(SCALE)==0 && (a/SCALE)%(SAMPLERATE/TICKSPERSEC)==0)
      genvol -= (genvol/64)*gendiminish;

    if (repeat)
      data = (SCALE - a%SCALE)*parser->samples[x].rptData[a/SCALE] + (a%SCALE)*parser->samples[x].rptData[(a/SCALE+1)%size];
    else
      data = (SCALE - a%SCALE)*parser->samples[x].origData[a/SCALE] + (a%SCALE)*parser->samples[x].origData[(a/SCALE+1)%size];
    data /= SCALE;

    ui->graph->addPoints(gendata*(genvol>>8), data*vol);
  }

  ui->graph->update();
}


//////////////////////////////////////////////////////////////////////////////
// SLOTS
void modConvert::saveFile()
{
  QString fileName;

  if (!parserActive)
    return;

  parser->convertData();
  fileName = QFileDialog::getSaveFileName(this, "Save File", "", "C header file (*.h);;All Files (*.*)");
  parser->saveMini(fileName.toStdString().c_str());
}

void modConvert::openFile()
{
  QString fileName;

  fileName = QFileDialog::getOpenFileName(this, "Open File", "", "Amiga MOD Files (*.mod);;All Files (*.*)");
  if (fileName != "")
  {
    // parse file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
      QMessageBox::critical(this, "Error", "Could not open file");
      return;
    }
    QByteArray data = file.readAll();
    file.close();
    if (parserActive)
    {
      parserActive = 0;
      delete parser;
      while (ui->sampleSelect->count() > 0)
        ui->sampleSelect->removeItem(0);
    }
    parser = new modParser(data);
    parserActive = 1;
    ui->actionSave->setEnabled(true); // enable Save command
    ui->origSizeLabel->setText("Original MOD size: " + QString::number(data.length()));
    ui->miniSizeLabel->setText("New miniMOD size: " + QString::number(parser->newsize));
    ui->titleLabel->setText("Title: "+QString(parser->title));
    ui->triButton->setEnabled(true);
    ui->sawButton->setEnabled(true);
    ui->squareButton->setEnabled(true);
    //ui->sineButton->setEnabled(true);
    ui->noiseButton->setEnabled(true);
    ui->diminishBox->setEnabled(true);
    ui->offsetBox->setEnabled(true);
    ui->volumeBox->setEnabled(true);
    ui->newTypeBox->setEnabled(true);
    ui->sampleSelect->setEnabled(true);
    ui->songPlayButton->setEnabled(true);
    ui->origPlayButton->setEnabled(true);
    ui->miniPlayButton->setEnabled(true);
    ui->stopSongButton->setEnabled(true);
    for (int a=1; a<MAX_SAMPLES; a++)
      if (parser->samples[a].length > 0)
      {
        sampleIndex[ui->sampleSelect->count()] = a;
        ui->sampleSelect->addItem(QString(parser->samples[a].name));
      }
  }
  // Now force a refresh of the fields
  ui->newTypeBox->setValue(2);
  ui->newTypeBox->setValue(1);
}


void modConvert::radioChange()
{
  int a, x;
  if (!parserActive)
    return;
  if (ui->triButton->isChecked())
    a=0;
  if (ui->sawButton->isChecked())
    a=1;
  if (ui->squareButton->isChecked())
    a=2;
  //if (ui->sineButton->isChecked())
  //  a=3;
  if (ui->noiseButton->isChecked())
  {
    ui->offsetBox->setEnabled(false);
    a=3;
  }
  else
    ui->offsetBox->setEnabled(true);
  x = ui->newTypeBox->value(); // which newType are we editing
  parser->newTypes[x].waveType = a;
  updateGraph();
}


void modConvert::sampleChange(int idx)
{
  int x = sampleIndex[idx];
  if (!parserActive)
    return;
  if (parser->samples[x].repeatLen > 0)
    ui->sampleLenLabel->setText("Length: "+QString::number(parser->samples[x].repeatLen));
  else
    ui->sampleLenLabel->setText("Length: "+QString::number(parser->samples[x].length));
  ui->sampleVolLabel->setText("Volume: "+QString::number(parser->samples[x].volume));
  if (parser->samples[x].repeatLen > 0)
    ui->sampleFineLabel->setText("Repeats: Yes");
  else
    ui->sampleFineLabel->setText("Repeats: No");
  ui->newTypeBox->setValue(parser->samples[x].whichType);
  if (ui->noiseButton->isChecked()) // stop pitch change from being locked out
    ui->offsetBox->setEnabled(false);
  else
    ui->offsetBox->setEnabled(true);
  updateGraph(); // On the off chance that nothing else updates it
}

void modConvert::newTypeChange(int idx)
{
  int x = sampleIndex[ui->sampleSelect->currentIndex()];
  if (!parserActive)
    return; // this can happen when loading a new file
  parser->samples[x].whichType = idx;
  ui->volumeBox->setValue(parser->newTypes[idx].volume);
  ui->offsetBox->setValue(parser->newTypes[idx].semitoneOffset);
  ui->diminishBox->setValue(parser->newTypes[idx].diminish);
  switch (parser->newTypes[idx].waveType)
  {
    case 0: ui->triButton->setChecked(true); break;
    case 1: ui->sawButton->setChecked(true); break;
    case 2: ui->squareButton->setChecked(true); break;
    //case 3: ui->sineButton->setChecked(true); break;
    case 3: ui->noiseButton->setChecked(true); break;
  }
  updateGraph();
  parser->convertData();
  ui->miniSizeLabel->setText("New miniMOD size: " + QString::number(parser->newsize));
}

void modConvert::volumeChange(int idx)
{
  int x; // just to make things easier to follow
  if (!parserActive)
    return;
  x = ui->newTypeBox->value(); // which newType are we editing
  parser->newTypes[x].volume = idx;
  updateGraph();
}

void modConvert::diminishChange(int idx)
{
  int x; // just to make things easier to follow
  if (!parserActive)
    return;
  x = ui->newTypeBox->value(); // which newType are we editing
  parser->newTypes[x].diminish = idx;
  updateGraph();
}

void modConvert::offsetChange(int idx)
{
  int x;
  if (!parserActive)
    return;
  x = ui->newTypeBox->value(); // which newType are we editing
  parser->newTypes[x].semitoneOffset = idx;
  updateGraph();
}


void modConvert::playOrig()
{
  QFile qf(FILENAME);
  int a, x = sampleIndex[ui->sampleSelect->currentIndex()];
  QByteArray arr; // keep the quieter sample here

  if (!parserActive)
    return; // this can happen when loading a new file
  if (parser->samples[x].repeatLen > 0)
    for(a=0; a<parser->samples[x].rptData.size(); a++)
      arr.append(parser->samples[x].rptData[a]>>3);
  else
    for(a=0; a<parser->samples[x].origData.size(); a++)
      arr.append(parser->samples[x].origData[a]>>3);
  if (parser->samples[x].repeatLen > 0)
    writeWav(arr, 1, parser->samples[x].volume);
  else
    writeWav(arr, 0, parser->samples[x].volume);
  qs->setLoops(1);
  qs->stop();
  qs->play();
//  if (qf.exists())
//    qf.remove();
}



void modConvert::playSynth()
{
  const float LENGTH = (1.2)*SAMPLERATE;
  const int SCALE = 64;
  QFile qf(FILENAME);
  QByteArray arr;
  int a,x = parser->samples[sampleIndex[ui->sampleSelect->currentIndex()]].whichType;
  float len = 64;
  int period, note=0, pos=0;
  int volume = (parser->newTypes[x].volume << 8);

  if (!parserActive)
    return; // this can happen when loading a new file
  for (a=0; a<parser->newTypes[x].semitoneOffset; a++)
    len /= 1.05946309;
  for (a=0; a>parser->newTypes[x].semitoneOffset; a--)
    len *= 1.05946309;
  period = SCALE*len;


  for (a=0; a<LENGTH; a++)
  {
    switch (parser->newTypes[x].waveType)
    {
      case 0: // triangle
        if (pos < period / 2)
          note = 255UL*(unsigned long)pos/(period/2);
        else
          note = 255 - 255UL*(unsigned long)(pos-period/2)/(period/2);
        break;
      case 1: // saw
        note = 255 - 255UL*(unsigned long)pos/period;
        break;
      case 2: // square
        if (pos<period/2)
          note = 0;
        else
          note = 255;
        break;
      //case 3: // sine
      //  note = 128+127*sin(pos*2*3.14159265/period);
      //  break;
      case 3: // noise
        note = qrand()%256;
        break;
    }
    note -= 128;
    note *= (volume>>8);
    note /= 64;
    if (a%(SAMPLERATE/TICKSPERSEC)==0) // once per tick
      volume -= (volume/64)*parser->newTypes[x].diminish;
    note >>= 3; // Because it's too LOUD!
    arr.append(note);
    pos += SCALE;
    if (pos>=period)
      pos -= period;
  }
  writeWav(arr, 0);
  qs->setLoops(1);
  qs->stop();
  qs->play();
//  if (qf.exists())
//    qf.remove();
}


void modConvert::playSong()
{
  QFile qf(FILENAME);

  parser->convertData();
  parser->makeWave();
  writeWav(parser->wave,0);
  qs->play();
//  if (qf.exists())
//    qf.remove();
}

void modConvert::stopSong()
{
  qs->stop();
}


void modConvert::writeWav(QByteArray arr, int repeat, int vol)
{
  const float SECONDS = 1.2;
  unsigned int a;
  unsigned int totalsize;
  FILE *fp;

  fp = fopen(FILENAME,"wb");
  fprintf(fp,"RIFF"); // Chunk ID
  if (repeat)
    totalsize = SECONDS*SAMPLERATE;
  else
    totalsize = arr.size();
  a = 36 + totalsize;
  fprintf(fp,"%c%c%c%c",a&0xFF,(a&0xFF00)>>8,(a&0xFF0000)>>16,(a&0xFF000000)>>24); // Chunksize (36 + SubChunk2Size)
  fprintf(fp,"WAVE"); // Format
  fprintf(fp,"fmt "); // Subchunk 1 ID
  fprintf(fp,"%c%c%c%c",16,0,0,0); // Subchunk 1 size
  fprintf(fp,"%c%c",1,0); // Audio format
  fprintf(fp,"%c%c",1,0); // Mono
  fprintf(fp,"%c%c%c%c",SAMPLERATE&0xFF,(SAMPLERATE&0xFF00)>>8,(SAMPLERATE&0xFF0000)>>16,(SAMPLERATE&0xFF000000)>>24); // sample rate
  fprintf(fp,"%c%c%c%c",SAMPLERATE&0xFF,(SAMPLERATE&0xFF00)>>8,(SAMPLERATE&0xFF0000)>>16,(SAMPLERATE&0xFF000000)>>24); // byte read rate
  fprintf(fp,"%c%c",1,0); // Block Align
  fprintf(fp,"%c%c",8,0); // Bits per sample
  fprintf(fp,"data");
  a = totalsize;
  fprintf(fp,"%c%c%c%c",a&0xFF,(a&0xFF00)>>8,(a&0xFF0000)>>16,(a&0xFF000000)>>24); // sample rate
  for (a=0; a<totalsize; a++)
    fputc(((arr[a%arr.size()]*vol)/64)+128,fp);
  fclose(fp);
}

