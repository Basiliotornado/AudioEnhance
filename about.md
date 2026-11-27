# Audio 

Patches for FMOD to improve the quality of songs. (Only tested on Windows/Linux!)

## Changes

- Currently, by some issue in FMOD, the mp3 loader used limits frequencies above 16khz, resulting in roughly 128-192kbps audio quality. My mod replaces FMOD's loader with drmp3, bypassing this issue and restoring quality for 256+kbps song files.

- The interpolation method used has also been swapped from linear to spline, reducing artifacts and attenuation of high frequencies.


[Pairs great with my HD recreations of Robtop songs!](https://drive.google.com/drive/folders/1vLT9Q4fcer7NYYA9M31VHznWEIFz0uJp)


(I hope this mod will be obsoleted at some point, since all FMOD projects using mp3 would benefit!)