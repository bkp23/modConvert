#ifndef MODPARSER_H
#define MODPARSER_H

#include <QByteArray>
#include <QList>

#define MAX_SAMPLES 32 // 1..31 (0 is unused)
#define MAX_TYPES 0x10 // 1..15 Samples turn into "types"

class modParser
{
  public:
    modParser(QByteArray data);
    ~modParser();

    /// Original MOD data
    char title[20];
    struct
    {
      // original MOD data
      char name[22];
      unsigned int length;
      int finetune;
      unsigned int volume;
      unsigned int repeatPos;
      unsigned int repeatLen;
      QByteArray origData; // Holds the original sample data
      // Computed data
      QByteArray rptData; // Hold on the repeated portion of the data
      int whichType;
    } samples[MAX_SAMPLES];
    unsigned int numPatterns;
    unsigned int maxPattern;
    unsigned char patternOrder[128];
    struct
    {
      int whichSample;
      int period;
      char effect;
      char param1; // effect parameter 1
      char param2; // effect parameter 2
      //  pattern, div, ch
    } division[128][64][4]; // array indicates pattern #

    /// New data
    struct
    {
      quint8 waveType; // (2 bits) 0=triangle, 1=sawtooth, 2=square, 3=noise
      qint8  semitoneOffset; // (8 bits) number of semitones to offset
      quint8 volume;   // (6 bits) from 0..64
      quint8 diminish; // (0..64)
    } newTypes[MAX_TYPES]; // instruments

    QList<int> divOrder; // The order of the original divisions
    QList<unsigned char> mininotes[4], minieffects[4]; // The actual notes/effects
    int newsize; // Size of miniMod in bytes
    QByteArray wave; // Used for saving the wave output (when playing song)

    void convertData();
    void makeWave();
    void saveMini(const char *filename);

  private:
    void computeData(); // computes newTypes[] and rptData
    qint8 findOffset(unsigned int len);
    quint8 findNote(unsigned int period);
    void streamData(); // Populates divOrder[]

    unsigned int maxtype; // total number of used "newTypes" (instruments)
};

#endif // MODPARSER_H
