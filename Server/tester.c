#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

char** strSplit(char* input, const char a_delim)
{
    char* a_str = malloc(strlen(input));
    strcpy(a_str,input);
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

void parseRequest(char* result[],char *request){
	//Only GET responses are supported
	//EX GET /page/info.html HTTP/1.1
    char messageType[128];
	char message[128];
	char** parts;
	char* subbuff;
	size_t len = 0;
	size_t read = 0;
	parts = strSplit(request, '/');
	if(parts){
        strcpy(messageType, *(parts + 1));
		strcpy(message, *(parts + 2));
		int len = strlen(message) - 4; //Doesn't take " HTTP" in consideration
		subbuff = malloc(sizeof(char*) *len);
		memcpy(subbuff, &message[0], len-1);
		subbuff[len-1] = '\0';
	}
    result[0] = malloc(sizeof(char*) *128);
    result[1] = malloc(sizeof(char*) *128);
    strcpy(result[0], messageType);
    strcpy(result[1], subbuff);
} 

int main(int argc, char* argv[]){
    char *result[2];
    parseRequest(result, "GET /page/info.html HTTP/1.1");
    printf("Message type %s and message %s",result[0],result[1]);
    return 0;
}