// #include "text_buffer.h"

void runcode(char* fname);
//void runcode(text_buffer* aTextBuffer);
void parseLine(char *command, char *param1, char *param2);
uint8_t lower(uint8_t num);
// Removes all occurrences of `sub` from a single string, in place.
void strip_substr(char *str, const char *sub);
void strip_substr_array(char *arr[], size_t count, const char *sub);