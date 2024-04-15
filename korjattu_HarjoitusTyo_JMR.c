#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

//Initializing global constants
#define MAX_WORD_LENGTH 100
#define TABLE_SIZE 200000

//Setting up a struct for the hash table
struct word_counts {
        char word[MAX_WORD_LENGTH];
        int count;
    } ;

//Setting up a hash table with structs
struct word_counts word_table[TABLE_SIZE];

//A word hashing function based on adler32 function
int hash(const char * word) {
    unsigned int a = 1, b = 0, i = 0, MOD = 65521;
    
    while (word[i] != '\0') {
        a = (a + word[i]) % MOD;
        b = (b + a) % MOD;
        i++;
    }
    return ((b << 16) | a)%TABLE_SIZE;
}

//This function is based on ex 5 on hash tables, modified to work with a struct array.
int insert_word(char *word){

    int place = hash(word);
    int firstplace = place;
    int stored = 0;

    while(stored==0){
        if(word_table[place].count == 0){
            //If the word wasn't in the hash table, we add it there, and add word count 1 to the word count list
            snprintf(word_table[place].word, MAX_WORD_LENGTH, word);
            word_table[place].count = 1;
            stored = 1;
        }
        else {
            if(strcmp(word_table[place].word, word) == 0){
                stored = 1;
                //If the word was already in the hash table, we update the increased word count to list
                word_table[place].count = word_table[place].count + 1;
            }
            else{          
                place = (place+1)%TABLE_SIZE;
                //Changing the hash table position when collision happens
            }
        }
        if(stored == 0 && firstplace == place){
            // Table was full
            stored = -1;
        }
    }

    return stored;
}

//Function for finding the most common words
int maxC(){
    int count = 0;
    int mxcount = 0;
    int mxhashpos = 0;
    printf("word frequencies\n");
    
    //We go through the hash table 100 times, each times finding the most common word, and setting it's word count to 0 afterwards.
    for (int i = 0; i < 100; i++) {
        for (int a = 0; a < TABLE_SIZE; a++) {
            if (word_table[a].count != 0) {
                count = word_table[a].count;
                
                if (count > mxcount) {
                    mxcount = count;
                    mxhashpos = a;
                }
            } 
        }
        
        word_table[mxhashpos].count = 0;
        printf("%d: \"%s\" count: %d\n",(i + 1), word_table[mxhashpos].word, mxcount);
        
        mxcount = 0;
        mxhashpos = 0;
    }
    
    return 1;
} 

int main(){
    char word[MAX_WORD_LENGTH] = "0";
    int fc = 0;
    int wordi = 0;
    FILE *wordfile;
    char filename[20] = "testi.txt";
    
    printf("Please type the name of the file: \n");
    scanf("%s", filename);
    
    //Starting timer. This is also based on the ex 5 on hash tables.
    clock_t start,end;
	double totaltime;
	printf("Starting \n");
	start = clock();
    
    
    printf("Opening file %s and calculating word frequencies...\n",filename);
    wordfile = fopen(filename,"r");

        if (wordfile == NULL){
		    printf("ERROR READING THE FILE!\n");
		    return 0;
	    } do {

                fc = fgetc(wordfile);
                
                //Exit the loop when the file ends, and finalize the last word
                if (feof(wordfile)) {
                    word[wordi] = '\0';
                    printf("Success!\n");
                    printf("End of file \n");
            	    break;
                }
                
                //We take one char at a time from the file, and see if it's an alphabet or a special marker.
                //Chars are combined into the array word, until a space, an enter, -, . or , is reached.
                if (((fc == ' ') || (fc == '\n') || (fc == '-') || (fc == '.') || (fc == ',')) && (wordi != 0)) {
                    if(word[wordi - 1] == '\'') {
                    	word[(wordi - 1)] = '\0';
                    }
                    word[wordi] = '\0';
                    if(insert_word(word) < 0){
                        printf("HASH TABLE FULL. CANCELING\n");
                        fclose(wordfile);
                        return 0;
                    }
                    wordi = -1;
                } else if (isalpha(fc)){
                    fc = tolower(fc);
                    word[wordi] = (char)fc;
                } 
                //allowing ' to be a part of a word, if it's in the middle of a word. Checker for ' in the beginning of a word is commented
                else if (fc == '\'' /**&& (wordi != 0)**/) { 
                    word[wordi] = (char)fc;
                } else {
                    wordi--;
                }
                wordi++;
        } while(1);

    fclose(wordfile);
    
    printf("Finding the 100 most common words \n");
    //Finding and printing the most common words.
    maxC();
    
    //Finishing timer.
    end = clock();
    totaltime = (double)(end-start)/CLOCKS_PER_SEC;
    printf("Done. Consumed time: %f seconds \n",totaltime);

    return 0;
}
