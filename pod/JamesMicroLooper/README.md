# MicroLooper

A scrollable looper, currently with a window size fixed to 200ms.   This project uses a Daisy Pod and libdaisy.

Pressing and holding the encoder engages the looping, turning it changes the start position of the loop (which wraps around the buffer).  The audio buffer is currently 1.5s long.

A tiny crossfade is implemented at the loop boundary to avoid clicks when looping near the start or end of the buffer. 

