//---
//	keys - Map number keys to digit values
//---

#ifndef CASIO_KEYS
#define CASIO_KEYS

/* key_digit(): 0-9 for the number keys, -1 for any other key */
int key_digit(int key);

/* key_hex(): 0-15: the number keys, then A-F on the keys that carry those
   letters in red (X,theta,T  log  ln  sin  cos  tan); -1 otherwise */
int key_hex(int key);

#endif /* CASIO_KEYS */
