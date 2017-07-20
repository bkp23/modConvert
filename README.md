# modConvert
A MOD-to-chiptunes converter


# === WHAT DOES IT DO? ===
Takes a .MOD file as input and converts it from sampled sounds to a chiptunes format.
This is intended for use in chiptunes devices or other applications where smaller filesize is desireable.
For an example of its use, [watch this video that I made of the program and a microprocessor application](https://www.youtube.com/watch?v=SsBf6Veq-Ps)

# === HOW DO I USE IT? ===
1) Run the Win32 executable in the "compiled" folder or build your own from the "src" folder.
2) Load a .MOD file. A couple examples are provided in the "example" folder
3) Use the pull-down menu to go through the MOD's tracks and determine which tracks you want to use
  a. Some "tracks" only contain author info and can be ignored by merging the "New miniMOD" instrument number with a necessary track
4) Play the sample of the original track and determine the best miniType match for it.
  a. The program tries to detect the correct putch by default, so often only minor adjustments are needed.
  b. The type of waveform (triangle, sawtooth, square, noise) will determine the type of sound. Play around to find one that works best.
  c. Volume should be adjust so the resulting waveform (red) is close to the original (green). Again, the program autodetects this setting, so few adjustments are needed.
  d. Some instruments diminish or "fade out" over time. This setting is also autodetected, but may be manually tweaked.
5) When you are happy with the results, save the file and use it in your application.

