#include "url_enc_dec.hpp"
#include <cstdio>
#include <errno.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/ioctl.h>
#endif

/* Variables */
static bool ENCODE   = true;
static bool FULL     = false;
static bool SPECIAL  = false;

static FILE *INFILE  = stdin;
static FILE *OUTFILE = stdout;

static url::encode::charset ENCODE_CHARSET = url::encode::charset::reserved;

static inline int __available_on_stdin(){
  unsigned long available = 0;
  #ifdef _WIN32
    static HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr);
  #else
    ioctl(0, FIONREAD, &available);
  #endif
  return available;
}

static inline long __available_on_file(FILE *file){
  fseek(file, 0, SEEK_END);
  long av = ftell(file);
  fseek(file, 0, SEEK_SET);
  return av;
};

static inline void usage(){
  printf(R"(** Gurl **
URL encode or decode data

** Usage **
./gurl [OPTIONS]

** Options **
Note: by default the program will encode reserved characters, the -r option
only makes sense if -s | --special is given and you also want to encode reserved characters.
  
-d, --decode      : Decode
-r, --reserved    : Encodes ASCII reserved characters %s
-s, --special     : Encodes ASCII special characters %s
-f, --full        : Encodes every character
-o, --out <file>  : Output data to file instead of stdout
-i, --in  <file>  : Read data from file instead of stdin
 
** Examples **
echo '/my_/$foo_example' | gurl       => %%2fmy_%%2f%%24foo_example
echo '/my_/$foo_example' | gurl -s    => /my%%5f/$foo%%5fexample
echo '/my_/$foo_example' | gurl -f    => %%2f%%6d%%79%%5f%%2f%%24%%66%%6f%%6f%%5f%%65%%78%%61%%6d%%70%%6c%%65%%0a
echo '/my_/$foo_example' | gurl -r -s => %%2fmy%%5f%%2f%%24foo%%5fexample
gurl -f -i /etc/passwd -o encoded
gurl -d -i encoded -o decoded
)",ASCII_RESERVED, ASCII_SPECIAL);
}

static int parse(int argc, const char **args){
  bool user_wants_reserved = false;

  for(int i = 1; i < argc; ++i){
    std::string_view opt = args[i];
    if("-d" == opt || "--decode" == opt){
      ENCODE = false;
    }else if("-f" == opt || "--full" == opt){
      FULL = true;
    }else if("-r" == opt || "--reserved" == opt){
      user_wants_reserved = true;
      ENCODE_CHARSET += url::encode::charset::reserved;
    }else if("-s" == opt || "--special" == opt){
      if(user_wants_reserved){
        ENCODE_CHARSET += url::encode::charset::special;  
      }else{
        ENCODE_CHARSET = url::encode::charset::special;
      }
    }else if("-h" == opt || "--help" == opt){
      usage(); return 1;
    }else if("-o" == opt || "--out" == opt){
      if(i+1 == argc){
        fprintf(stderr, "-- Missing argument for '%s'",opt.data()); return 1;
      }
      OUTFILE = fopen(args[++i], "wb");
      if(OUTFILE == nullptr){
        perror("-- Couldn't open OUTPUT file");
        return 1;
      }
    }else if("-i" == opt || "--infile" == opt){
      if(i+1 == argc){
        fprintf(stderr, "-- Missing argument for '%s'",opt.data()); return 1;
      }
      INFILE = fopen(args[++i], "rb");
      if(INFILE == nullptr){
        perror("-- Couldn't open INPUT file");
        return 1;
      }
    }else{
      fprintf(stderr, "-- Unknown option '%s'\n", args[i]);
    }
  }

  return 0;
}

int main(int argc, const char **args) {
  if(parse(argc, args)){
    return -1;
  }

  long long available = 0;

  available = INFILE == stdin ? __available_on_stdin() : __available_on_file(INFILE);
  
  if(available <= 0){
    fprintf(stderr, "[!] Got nothing in stdin to %s, use -h | --help to display help\n", ENCODE ? "encode" : "decode");
    return 1;
  }

  unsigned char* data = (unsigned char*)malloc(available);

  if(data == nullptr){
    fprintf(stderr, "[!] Couldn't allocate enough memory\n");
    return 1;
  }

  if(fread(data, available, 1, INFILE) != 1){
    if(ferror(INFILE)){
      perror("Reading data to buffer");
      return 1;
    }
  };

  std::string result;

  if(ENCODE){
    if(FULL == false && ENCODE_CHARSET == url::encode::charset::none){
      fprintf(stderr, "[!] No encoding option given, use -h | --help to see available options\n");
      return 1;
    }
    result = FULL ? url::encode::full(reinterpret_cast<const unsigned char*>(data), available, OUTFILE)
                  : url::encode::partial(reinterpret_cast<const unsigned char*>(data), available, ENCODE_CHARSET, OUTFILE);
  }else{
    result = url::decode::full(reinterpret_cast<const unsigned char*>(data), available, OUTFILE);
  }

  if(INFILE != stdin){
    fclose(INFILE);
  }

  if(OUTFILE != stdout){
    fclose(OUTFILE);
  }

  free(data);

  return 0;
}