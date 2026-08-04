// FUNCTIONS IN RUNTIME.H

void runcode(char* fname);
void parseLine(char *command, char *param1, char *param2, char *param3, char *param4);
uint8_t lower(uint8_t num);
void strip_substr(char *str, const char *sub);
void strip_substr_array(char *arr[], size_t count, const char *sub);
void replace_char(char *str, char old_char, char new_char);
void toUpperCase(char *str) ;
uint16_t find_string(char *arr[], uint16_t size, char *target);
void strip_leading_spaces_inplace(char *str) ;
char *strip_leading_spaces(char *str);
