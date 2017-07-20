#include "modparser.h"
#include <QFile>

modParser::modParser(QByteArray data)
{
  unsigned int a, b, c, idx=0;

  for (a=0;a<20; a++) // Title comes first
    title[a] = data[idx++];
  for (a=1; a<MAX_SAMPLES; a++) // Then the sample data
  {
    for (b=0; b<22; b++)
      samples[a].name[b]=data[idx++];
    samples[a].length = ((unsigned char)data[idx++]<<8);
     samples[a].length |= (unsigned char)data[idx++];
     samples[a].length *= 2;
    samples[a].finetune = data[idx++];
    if (samples[a].finetune>=0x08)
      samples[a].finetune -= (0xF+1);
    samples[a].volume = data[idx++];
    samples[a].repeatPos = ((unsigned char)data[idx++]<<8);
     samples[a].repeatPos |= (unsigned char)data[idx++];
     samples[a].repeatPos *= 2;
    samples[a].repeatLen = ((unsigned char)data[idx++]<<8);
     samples[a].repeatLen |= (unsigned char)data[idx++];
     samples[a].repeatLen *= 2;
    if (samples[a].repeatLen <= 2)
      samples[a].repeatLen = 0;
  }
  numPatterns = data[idx++];
  idx++; // ignore "restart byte"
  maxPattern = 0;
  for (a=0; a<128; a++)
  {
    patternOrder[a] = data[idx++];
    if (patternOrder[a] > maxPattern && a<numPatterns)
      maxPattern = patternOrder[a];
  }
  idx += 4; // skip "M.K."

  // 1024 bytes for each pattern (from 1..maxPattern)
  for (a=0; a<=maxPattern; a++) // patterns
    for (b=0; b<64; b++) // divisions
      for (c=0; c<4; c++) // channels
      {
        division[a][b][c].whichSample = (data[idx]&0xF0) | ((data[idx+2]&0xF0)>>4);
        division[a][b][c].period = ((data[idx]&0x0F)<<8) | (unsigned char)data[idx+1];
        division[a][b][c].effect = (data[idx+2] & 0x0F);
        division[a][b][c].param1 = ((data[idx+3] & 0xF0) >> 4);
        division[a][b][c].param2 = (data[idx+3] & 0x0F);
        idx += 4; // skip to next channel
      }
  // sample[a].length bytes for each sample
  for (a=1; a<MAX_SAMPLES; a++)
    for (b=0; b<samples[a].length; b++)
      samples[a].origData.append(data[idx++]);

  computeData(); // computes newTypes[] and rptData
  streamData();  // populates divOrder[]
  convertData();
}

modParser::~modParser()
{ }


void modParser::computeData()
{
  unsigned int a, b, typeCount=1;

  for (a=1; a<MAX_SAMPLES; a++)
  {
    // Extract repeated portion of data
    for (b=samples[a].repeatPos; b<samples[a].repeatPos+samples[a].repeatLen && b<samples[a].length; b++)
      samples[a].rptData.append(samples[a].origData[b]);
    // Assign the sample a newType
    if (samples[a].length > 0)
    {
      samples[a].whichType = typeCount;
      newTypes[typeCount].waveType = 0;
      if (samples[a].repeatLen > 0)
      {
        newTypes[typeCount].diminish = 0;
        newTypes[typeCount].semitoneOffset = findOffset(samples[a].repeatLen);
      }
      else
      {
        newTypes[typeCount].diminish = 5;
        newTypes[typeCount].semitoneOffset = 0;
      }
      newTypes[typeCount].volume = samples[a].volume;
      if (typeCount+1 < MAX_TYPES)
        typeCount++;
      // Assign a name if none exists
      if (samples[a].name[0]=='\0')
      {
        samples[a].name[0] = '"';
        samples[a].name[1] = 'A' + a;
        samples[a].name[2] = '"';
        samples[a].name[3] = '\0';
      }
    }
    else
      samples[a].whichType = 0; // this sample is empty
  }
}


qint8 modParser::findOffset(unsigned int len)
{
  // offset=0 when len=64 (len is guaranteed to be even)
  const float MULT = 1.05946309; // 12th root of 2
  const int GOAL = 64;
  qint8 prevOffset=0, offset = 0;
  float size = (float)len;
  float prevDist = 0xFFFFF;

  // Approximate the number of half-steps to reach C3
  while (prevDist*prevDist  >  (GOAL-size)*(GOAL-size))
  {
    prevOffset = offset;
    prevDist = (GOAL-size);
    if (size > GOAL)
    {
      size /= MULT; // lower size of origSample
      offset--;
    }
    else
    {
      size *= MULT; // raise size of origSample
      offset++;
    }
  }

  if (prevOffset < -50)
    return -50;
  if (prevOffset > 50)
    return 50;
  return prevOffset;
}

quint8 modParser::findNote(unsigned int period) //returns 0, 1..72
{
  const unsigned int PRDS[] =
    {1712,1616,1525,1440,1357,1281,1209,1141,1077,1017,961, 907, // C0-B0
     856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, // C1-B1
     428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, // C2-B2
     214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113, // C3-B3
     107, 101,  95,  90,  85,  80,  76,  71,  67,  64,  60,  57, // C4-B4
     53,  50,  47,  45,  42,  40,  38,  35,  33,  32,  30,  28}; // C5-B5
  int a;
  if (period <20)
    return 0; // no note
  for(a=0;PRDS[a]>period && a<60;a++);
  if (a==0)
    return 0+1;
  if (a==60)
    return 59+1;
  if (period-PRDS[a-1] < period-PRDS[a])
    return a-1+1;
  return a+1;
}


void modParser::streamData() // Populate divOrder[]
{
  int ch;
  unsigned int patternIdx=0, div=0, ptrn;
  unsigned int nextDiv;

  while (patternIdx < numPatterns)
  {
    ptrn = patternOrder[patternIdx];
    divOrder.append(ptrn*128+div); // record our visit to this div
    nextDiv = div+1;
    for (ch=3; ch>=0; ch--)
    {
      if (division[ptrn][div][ch].effect == 0xE && division[ptrn][div][ch].param1 == 0xE)
      { // delay pattern
        if (division[ptrn][div][ch].param2 > 0)
        {
          division[ptrn][div][ch].param2--; // destructive!
          nextDiv = div;
        }
      }
      if (division[ptrn][div][ch].effect == 0xD)
      { // pattern break
        patternIdx++;
        nextDiv = 10*division[ptrn][div][ch].param1 + division[ptrn][div][ch].param2;
      }
    }
    div = nextDiv;
    if (div >= 64)
    {
      div = 0;
      patternIdx++;
    }
  } // end while(patterns)
} // end streamData()


void modParser::convertData() // populate mininotes, minieffects
{
  QList<unsigned int> notelist[4], effectlist[4];
  int a, ch;
  // note/type stuff
  unsigned int lastnote, lasttype;
  int sample, note, type;
  // effect stuff
  unsigned int effect, p1, p2;
  int lastslide1, lastslide2;
  // and size stuff
  maxtype=0;

  // STEP ONE: convert the streamed data into supported formats
  for (ch=0; ch<4; ch++)
  {
    lastnote = 0;
    lasttype = 0;
    lastslide1 = 0; lastslide2 = 0;
    for (a=0; a<divOrder.size(); a++)
    {
      // Figure out the next note/type
      sample = division[divOrder[a]/128][divOrder[a]%128][ch].whichSample;
      if (sample==0)
        type = 0;
      else
        type = samples[sample].whichType;
      if (type != 0)
        lasttype = type;
      note = findNote(division[divOrder[a]/128][divOrder[a]%128][ch].period);
      if (note==0)
        note = lastnote;
      else
      {
        if (type != 0)
          note += newTypes[type].semitoneOffset;
        if (note<0)
          note = 0;
        if (note>72)
          note = 72;
        //type = lasttype; // in case type was 0--we want to reload the sample
      }
      lastnote = note;
      notelist[ch].append((type << 8) | note);

      // Figure out the next effect
      effect = division[divOrder[a]/128][divOrder[a]%128][ch].effect;
      p1 = division[divOrder[a]/128][divOrder[a]%128][ch].param1;
      p2 = division[divOrder[a]/128][divOrder[a]%128][ch].param2;
      switch (effect)
      {
        case 0x0: break; // Arpeggio      (use verbatim)
        case 0x1: break; // Slide up      (use verbatim)
        case 0x2: break; // Slide down    (use verbatim)
        case 0x3:        // Slide to note (use verbatim)
          if (p1==0 && p2==0)
          {
            p1 = lastslide1;
            p2 = lastslide2;
          }
          lastslide1 = p1;
          lastslide2 = p2;
          break;
        case 0x5: break; // [3]+volume slide (use verbatim)
        case 0x6:        // [4]+volume slide -- convert to [A]
          effect = 0xA; // params are the same
          break;
        case 0xA: break; // Volume slide  (use verbatim)
        case 0xC: break; // Volume set    (ese verbatim)
        case 0xE: // This includes a slew of other effects
          if (p1!=0xA  && p1!=0xB && p1!=0xC) // these 3 are verbatim
          { // everything else we ignore
            effect = 0;
            p1 = 0;
            p2 = 0;
          }
          if (p1==0xC) // unsupported
          {
            effect = 0xC;
            p1 = 0;
            p2 = 0;
          }
          break;
        case 0xF:        // Set speed
          if (p1*16+p2 > 32) // we don't support set tempo
          {
            effect = 0;
            p1 = 0;
            p2 = 0;
          }
          if (p1==0 && p2==0)
            p2 = 1;
          break;
        default: // unsupport effect
          effect = 0;
          p1 = 0;
          p2 = 0;
      } // end switch(effect)
      if (effect == 0 && p1 == 0 && p2 == 0)
        effect = 0x8; // It's our special "no effect" effect
      effectlist[ch].append((effect<<8) | ((p1&0xF)<<4) | (p2&0xF));
    } // end for(divOrder)
  } // end for(ch)

  // STEP TWO: Compress supported format into mininotes/minieffects
  for(ch=0; ch<4; ch++)
  {
    // First do mininotes
    while (!mininotes[ch].empty()) // empty out mininotes (from previous runs)
      mininotes[ch].removeFirst();
    while (!notelist[ch].empty())
    {
      a=0;
      lastnote = notelist[ch][0];
      notelist[ch].removeFirst();
      while (a<15 && !notelist[ch].empty() && notelist[ch][0]==lastnote)
      {
        a++;
        notelist[ch].removeFirst();
      }
      mininotes[ch].append((a<<4) | ((lastnote&0x0F00)>>8));
      mininotes[ch].append(lastnote & 0x00FF);
      if ((lastnote & 0x0F00) > maxtype)
        maxtype = (lastnote & 0x0F00);
    } // end note conversion
    // Then do minieffects
    while (!minieffects[ch].empty())
      minieffects[ch].removeFirst();
    while (!effectlist[ch].empty())
    {
      a=0;
      effect = effectlist[ch][0];
      effectlist[ch].removeFirst();
      while (a<15 && !effectlist[ch].empty() && effectlist[ch][0]==effect)
      {
        a++;
        effectlist[ch].removeFirst();
      }
      minieffects[ch].append((a<<4) | ((effect & 0x0F00)>>8));
      if ((effect & 0x0F00) != 0x0800) // there's no params for nop() effects
        minieffects[ch].append(effect & 0x00FF);
    } // end effect conversion
  } // end for(ch) (step two)
  maxtype >>= 8;
  newsize = maxtype*4; // each type (instrument) uses 4 bytes
  for (a=0; a<4; a++)
    newsize += minieffects[a].size() + mininotes[a].size();
} // end convertData()



void modParser::saveMini(const char *filename)
{
  char name[20];
  int a,ch,pos=0;
  FILE *fp;

  for (a=0; title[a]!='\0' && a<19; a++)
    if ( (title[a]>='A' && title[a]<='Z') || (title[a]>='a' && title[a]<='z'))
      name[pos++] = title[a];
  name[pos] = '\0';

  fp = fopen(filename,"wb");
  fprintf(fp,"/*********************************************************/\n");
  fprintf(fp,"/**  MiniMod song file                                  **/\n");
  fprintf(fp,"/**                                                     **/\n");
  fprintf(fp,"/**  Title: %-20s                        **/\n",title);
  fprintf(fp,"/**                                                     **/\n");
  fprintf(fp,"/**  Size (bytes): %-10d                           **/\n",newsize);
  fprintf(fp,"/**  Name: %-20s                         **/\n",name);
  fprintf(fp,"/*********************************************************/\n\n");
  fprintf(fp,"#ifndef MINIMOD_%s\n",name);
  fprintf(fp,"#define MINIMOD_%s\n\n",name);
  fprintf(fp,"#ifndef MIDIMOD_STRUCTS\n");
  fprintf(fp,"#define MIDIMOD_STRUCTS\n");
  fprintf(fp,"typedef struct\n");
  fprintf(fp,"  {\n");
  fprintf(fp,"    unsigned char waveType;       // (2 bits) 0=triangle, 1=sawtooth, 2=square, 3=noise\n");
  fprintf(fp,"             char semitoneOffset; // (8 bits) number of semitones to offset\n");
  fprintf(fp,"    unsigned char volume;         // (6 bits) from 0..64\n");
  fprintf(fp,"    unsigned char diminish;       // (0..64)\n");
  fprintf(fp,"  } TypeStruct;\n");
  fprintf(fp,"  typedef struct\n");
  fprintf(fp,"  {\n");
  fprintf(fp,"    unsigned int numNotes[4];\n");
  fprintf(fp,"    unsigned int numEffects[4];\n");
  fprintf(fp,"  } MidiModStats;\n");
  fprintf(fp,"  typedef struct\n");
  fprintf(fp,"  {\n");
  fprintf(fp,"    const rom unsigned char *notes[4];\n");
  fprintf(fp,"    const rom unsigned char *effects[4];\n");
  fprintf(fp,"    const rom TypeStruct *types;\n");
  fprintf(fp,"    const rom MidiModStats *stats;\n");
  fprintf(fp,"  } SongData;\n");
  fprintf(fp,"#endif\n\n");
  fprintf(fp,"const rom MidiModStats %sStats = { {%d, %d, %d, %d}, {%d, %d, %d, %d} };\n\n",name,
          mininotes[0].size(),  mininotes[1].size(),  mininotes[2].size(),  mininotes[3].size(),
          minieffects[0].size(),minieffects[1].size(),minieffects[2].size(),minieffects[3].size());
  fprintf(fp,"const rom TypeStruct %sTypes[%d] = {",name,maxtype);
  for(a=0; (unsigned int)a<maxtype; a++)
    fprintf(fp,"\n    { %d, %-4d, %-2d, %-3d},",
            newTypes[a].waveType,
            newTypes[a].semitoneOffset,
            newTypes[a].volume,
            newTypes[a].diminish);
  fprintf(fp,"  };\n\n");
  /*
  fprintf(fp,"const int %sDivOrder[%d] =\n  { ", name, divOrder.size());
  for(a=0; a<divOrder.size(); a++)
  {
    if (a != 0)
      fprintf(fp,", ");
    if (a>0 && a%20==0)
      fprintf(fp,"\n    ");
    fprintf(fp, "0x%04X",divOrder[a]);
  }
  fprintf(fp," };\n\n");
  */
  for (ch=0; ch<4; ch++)
  {
    fprintf(fp,"const rom unsigned char %sNotesCh%d[%d] =\n  { ", name, ch+1, mininotes[ch].size());
    for(a=0; a<mininotes[ch].size(); a++)
    {
      if (a != 0)
        fprintf(fp,", ");
      if (a>0 && a%27==0)
        fprintf(fp,"\n    ");
      fprintf(fp, "0x%02X",mininotes[ch][a]);
    }
    fprintf(fp," };\n\n");
  }
  for (ch=0; ch<4; ch++)
  {
    fprintf(fp,"const rom unsigned char %sEffectsCh%d[%d] =\n  { ", name, ch+1, minieffects[ch].size());
    for(a=0; a<minieffects[ch].size(); a++)
    {
      if (a != 0)
        fprintf(fp,", ");
      if (a>0 && a%27==0)
        fprintf(fp,"\n    ");
      fprintf(fp, "0x%02X",minieffects[ch][a]);
    }
    fprintf(fp," };\n\n");
  }
  fprintf(fp,"const rom SongData %s = {\n",name);
  fprintf(fp,"  { %sNotesCh1,   %sNotesCh2,   %sNotesCh3,   %sNotesCh4   },\n",name,name,name,name);
  fprintf(fp,"  { %sEffectsCh1, %sEffectsCh2, %sEffectsCh3, %sEffectsCh4 },\n",name,name,name,name);
  fprintf(fp,"  %sTypes,\n",name);
  fprintf(fp,"  &%sStats };\n\n",name);
  fprintf(fp,"#endif\n");

  fclose(fp);
}


///////////////////////////////// SIMULATOR

typedef struct
{
  unsigned char diminish, type;
  unsigned char phaseIncIdx, slideVal;
  unsigned char effect, param1, param2;
  unsigned int vol, phase, phaseIncrement;
  int noteidx, effectidx;
  int noterpt, effectrpt;
} joChannel;

void modParser::makeWave()
{
  const int NUM_CHANNELS = 4;
  const unsigned int PHASE_INC[73] = { 0, // index [0] is not a note
    0x0100, 0x010F, 0x011F, 0x0130, 0x0143, 0x0156, 0x016A, 0x0180, 0x0196, 0x01AF, 0x01C8, 0x01E3, // C0-B0
    0x0200, 0x021E, 0x023F, 0x0261, 0x0285, 0x02AB, 0x02D4, 0x02FF, 0x032D, 0x035D, 0x0390, 0x03C7, // C1-B1
    0x0400, 0x043D, 0x047D, 0x04C2, 0x050A, 0x0557, 0x05A8, 0x05FE, 0x0659, 0x06BA, 0x0721, 0x078D, // C2-B2
    0x0800, 0x087A, 0x08FB, 0x0983, 0x0A14, 0x0AAE, 0x0B50, 0x0BFD, 0x0CB3, 0x0D74, 0x0E41, 0x0F1A, // C3-B3
    0x1000, 0x10F4, 0x11F6, 0x1307, 0x1429, 0x155C, 0x16A1, 0x17F9, 0x1966, 0x1AE9, 0x1C82, 0x1E34, // C4-B4
    0x2000, 0x21E7, 0x23EB, 0x260E, 0x2851, 0x2AB7, 0x2D41, 0x2FF2, 0x32CC, 0x35D1, 0x3904, 0x3C68};// C5-B5
  const int SLICES_PER_TICK = 166;
  joChannel channel[4];
  unsigned short lfsr = 0xACE1u;
  int ticksPerDiv = 6;
  int tick = 0, tickMod3 = 0;
  int slice = 0;
  unsigned int note=0; // used in synthesizing the sound
  int finalnote;     // "
  int a, whichChannel;

  for (whichChannel=0; whichChannel<NUM_CHANNELS; whichChannel++) // init channels
  {
    channel[whichChannel].diminish = 0;
    channel[whichChannel].type = 0;
    channel[whichChannel].vol = 30;
    channel[whichChannel].phaseIncIdx = 0;
    channel[whichChannel].noteidx = 0; channel[whichChannel].noterpt = 0;
    channel[whichChannel].effectidx = 0; channel[whichChannel].effectrpt = 0;
  }

  wave.clear();
  while (channel[0].noteidx < mininotes[0].size() || channel[0].noterpt>0)
  {
    // tick happens here //////////////////////////////////////////////////
    for (whichChannel=0; whichChannel<NUM_CHANNELS; whichChannel++)
      channel[whichChannel].vol -= (channel[whichChannel].vol/64)*channel[whichChannel].diminish;
    if (tick <= 0) // end of division; load a new one
    {
      for (whichChannel=0; whichChannel<NUM_CHANNELS; whichChannel++)
      {
        if (channel[whichChannel].effect==0) // if we were alternating notes
          channel[whichChannel].phaseIncrement = PHASE_INC[channel[whichChannel].phaseIncIdx]; // restore our default
        // load a new effect if necessary
        if (channel[whichChannel].effectrpt==0)
        {
          channel[whichChannel].effectrpt = (minieffects[whichChannel][channel[whichChannel].effectidx]&0xF0)>>4;
          channel[whichChannel].effect = minieffects[whichChannel][channel[whichChannel].effectidx] & 0x0F;
          channel[whichChannel].effectidx++; // pop repeat/effect
          if (channel[whichChannel].effect != 0x8) // if this effect has parameters
          {
            channel[whichChannel].param1 = (minieffects[whichChannel][channel[whichChannel].effectidx]&0xF0)>>4;
            channel[whichChannel].param2 = minieffects[whichChannel][channel[whichChannel].effectidx] & 0x0F;
            channel[whichChannel].effectidx++; // pop parameters
          }
          if (channel[whichChannel].effect == 0xF)
            ticksPerDiv = 16*channel[whichChannel].param1+channel[whichChannel].param2;
        }
        else
          channel[whichChannel].effectrpt--;
        if (channel[whichChannel].effect == 0xE)
        {
          if (channel[whichChannel].param1 == 0xA) // imm. increase volume
          {
            channel[whichChannel].vol += (channel[whichChannel].param2<<8);
            if (channel[whichChannel].vol > (64<<8))
              channel[whichChannel].vol = (64<<8);
          }
          else if (channel[whichChannel].param1 == 0xB) // imm. decrease volume
          {
            if (channel[whichChannel].vol > (unsigned int)(channel[whichChannel].param2<<8))
              channel[whichChannel].vol -= (channel[whichChannel].param2<<8);
            else
              channel[whichChannel].vol = 0;
          }
        } // end if (vol change)
        // Load a new note, if necessary
        if (channel[whichChannel].noterpt==0)
        {
          channel[whichChannel].noterpt = (mininotes[whichChannel][channel[whichChannel].noteidx]&0xF0)>>4;
          a = mininotes[whichChannel][channel[whichChannel].noteidx] & 0x0F; // the type
          channel[whichChannel].noteidx++; // pop repeat/sample
          channel[whichChannel].phaseIncIdx = mininotes[whichChannel][channel[whichChannel].noteidx];
          if (channel[whichChannel].effect != 3 && channel[whichChannel].effect != 5 && channel[whichChannel].effect != 1 && channel[whichChannel].effect != 2) // sliding
            channel[whichChannel].phaseIncrement = PHASE_INC[channel[whichChannel].phaseIncIdx];
          if (channel[whichChannel].effect == 3)
          {
            if (channel[whichChannel].phaseIncrement > PHASE_INC[channel[whichChannel].phaseIncIdx])
              channel[whichChannel].slideVal = channel[whichChannel].param1*16+channel[whichChannel].param2;
            else
              channel[whichChannel].slideVal = channel[whichChannel].param1*16+channel[whichChannel].param2;
          }
          channel[whichChannel].noteidx++; // pop note
          if (a)
          {
            channel[whichChannel].vol      = (newTypes[a].volume << 8);
            channel[whichChannel].diminish =  newTypes[a].diminish;
            channel[whichChannel].type     =  newTypes[a].waveType;
          }
        }
        else
          channel[whichChannel].noterpt--;
        if (channel[whichChannel].effect == 0xC)
          channel[whichChannel].vol = (16*channel[whichChannel].param1+channel[whichChannel].param2)<<8;
      } // end for(whichChannel)
      tick = ticksPerDiv;
      tickMod3 = 0;
    } // end if (new division)
    else // we're mid-division
    {
      tickMod3++;
      if (tickMod3 >= 3)
        tickMod3 = 0;
      for (whichChannel=0; whichChannel<4; whichChannel++)
      {
        switch (channel[whichChannel].effect)
        {
          case 0x0: // switch among notes
            if (tickMod3 == 0)
              channel[whichChannel].phaseIncrement = PHASE_INC[channel[whichChannel].phaseIncIdx];
            else if (tickMod3 == 1)
              channel[whichChannel].phaseIncrement = PHASE_INC[channel[whichChannel].phaseIncIdx+channel[whichChannel].param1];
            else if (tickMod3 == 2)
              channel[whichChannel].phaseIncrement = PHASE_INC[channel[whichChannel].phaseIncIdx+channel[whichChannel].param2];
            break;
          case 0x1: // decrease period (by increasing inc)
            channel[whichChannel].phaseIncrement += (16*channel[whichChannel].param1+channel[whichChannel].param2)*channel[whichChannel].phaseIncrement*channel[whichChannel].phaseIncrement/477737;
            break;
          case 0x2: // increase period (by decreasing inc)
            channel[whichChannel].phaseIncrement -= (16*channel[whichChannel].param1+channel[whichChannel].param2)*channel[whichChannel].phaseIncrement*channel[whichChannel].phaseIncrement/850000;
            break;
          case 0x5: // Volume slide, then 0x3
            // same code from 0xA here
            channel[whichChannel].vol += channel[whichChannel].param1<<8;
            if (channel[whichChannel].vol > (unsigned int)(channel[whichChannel].param2<<8))
              channel[whichChannel].vol -= channel[whichChannel].param2<<8;
            else
              channel[whichChannel].vol = 0;
            if (channel[whichChannel].vol > (64<<8))
              channel[whichChannel].vol = (64<<8);
            // no break
          case 0x3: // slide towards phaseIncIdx (but not past)
            // first, determine if we're sliding up or down
            if (channel[whichChannel].phaseIncrement > PHASE_INC[channel[whichChannel].phaseIncIdx])
            {
              channel[whichChannel].phaseIncrement -= channel[whichChannel].slideVal*channel[whichChannel].phaseIncrement*channel[whichChannel].phaseIncrement/850000;
              if (channel[whichChannel].phaseIncrement < PHASE_INC[channel[whichChannel].phaseIncIdx])
                channel[whichChannel].phaseIncrement = PHASE_INC[channel[whichChannel].phaseIncIdx];
            }
            else
            {
              channel[whichChannel].phaseIncrement += channel[whichChannel].slideVal*channel[whichChannel].phaseIncrement*channel[whichChannel].phaseIncrement/477737;
              if (channel[whichChannel].phaseIncrement > PHASE_INC[channel[whichChannel].phaseIncIdx])
                channel[whichChannel].phaseIncrement = PHASE_INC[channel[whichChannel].phaseIncIdx];
            }
            break;
          case 0xA: // Volume slide only (like 0x5)
            channel[whichChannel].vol += channel[whichChannel].param1<<8;
            if (channel[whichChannel].vol > (unsigned int)(channel[whichChannel].param2<<8))
              channel[whichChannel].vol -= channel[whichChannel].param2<<8;
            else
              channel[whichChannel].vol = 0;
            if (channel[whichChannel].vol > (64<<8))
              channel[whichChannel].vol = (64<<8);
            break;
        } // end switch(effect)
      } // end for(whichChannel)
    } // end else(mid-division)
    tick--;

    // slices until the next tick ///////////////////////////////////////
    for (slice = 0; slice < SLICES_PER_TICK; slice++)
    {
      finalnote = 0;
      for(whichChannel=0; whichChannel<NUM_CHANNELS; whichChannel++)
      {
        switch (channel[whichChannel].type)
        {
          case 0: // triangle
            note = (channel[whichChannel].phase & 0x7F80)<<1;
            if (channel[whichChannel].phase & 0x8000)
              note = 0xFF00-note;
            note >>= 8;
            break;
          case 1: // sawtooth
            note = ((channel[whichChannel].phase & 0xFF00)>>8);
            break;
          case 2: // square
            if (channel[whichChannel].phase & 0x8000)
              note = 0xFF;
            else
              note = 0;
            break;
          case 3: // noise
            lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
            note = (lfsr & 0xFF);
            break;
        }
        channel[whichChannel].phase += channel[whichChannel].phaseIncrement;
        note -= 128; // needed for signed array   ( -128..127 )
        note *= (channel[whichChannel].vol>>8); //(-8192..8128)
        note >>= 6;          // take note back to ( -128..127 )
        note >>= 2;     // adjust for other notes (  -32..31  ) ** Adjust for total range
        finalnote += (char)(note & 0xFF);
      }
      wave.append(finalnote);
    } // end for(slices)
  } // end while (notes)
} // end makeWave()
