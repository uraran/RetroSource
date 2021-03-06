#ifndef CHEATS_H
#define CHEATS_H

struct CheatsData {
  int code;
  int size;
  int status;
  bool enabled;
  u32 rawaddress;
  u32 address;
  u32 value;
  u32 oldValue;
  char codestring[20];
  char desc[32];
};

void cheatsAdd(const char *codeStr, const char *desc, u32 rawaddress, u32 address, u32 value, int code, int size);
void cheatsAddCheatCode(const char *code, const char *desc);
bool cheatsAddGSACode(const char *code, const char *desc, bool v3, bool encrypted);
bool cheatsAddCBACode(const char *code, const char *desc);
bool cheatsImportGSACodeFile(const char *name, int game, bool v3);
void cheatsDelete(int number, bool restore);
void cheatsDeleteAll(bool restore);
void cheatsEnable(int number);
void cheatsDisable(int number);
int cheatsCheckKeys(u32 keys, u32 extended);

extern int cheatsNumber;
extern CheatsData cheatsList[100];

#endif // CHEATS_H
