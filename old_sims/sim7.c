// $Id: sim7.c,v 1.1 2007/02/13 18:48:22 jamesll Exp jamesll $

/*
 * $Log: sim7.c,v $
 * Revision 1.1  2007/02/13 18:48:22  jamesll
 * Initial revision
 *
 * Revision 1.1  2007/01/24 05:58:46  jamesll
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long counter_t;
typedef unsigned long long code_t;

// --- function defs ---
void iterate(int depth, int nodei, int liveblocks);
FILE *myfopen(char *filename, char *mode);
int myclose(FILE *dafile);

// --- data structs ---
struct blockNode{
  int alive;

  code_t code;
};

// --- globals ---
int N, numTestBlocks;
counter_t livenodes, dednodes;

struct blockNode **blocks, *baseBlocks, **testBlocks;

// --- begin main() ---
int main(int argc, char **argv){
  FILE *infile;
  char *layoutfilename;

  int nodecount, maxnodes, maxdepth, error, num;
  code_t codei;
  
  int i, j, k, q;

  if(argc < 3){
    printf("Usage: %s ", argv[0]);
    printf("<layout file> <width>\n");
    printf("  use '-' for standard in.\n");
    return 1;
  }else{
    layoutfilename = argv[1];
    N = atoi(argv[2]);
  }

  infile = myfopen(layoutfilename, "r");

  nodecount = 0;
  numTestBlocks = 0;
  maxnodes = 1 << N;

  baseBlocks = calloc(maxnodes, sizeof(struct blockNode));

  //baseBlocks[0].alive = 0;

  for(i = 1; i < maxnodes; i <<= 1){
    baseBlocks[i].code = i;
  }

  // -- layout --
  while((error = fscanf(infile, "%llu %d", &codei, &num)) == 2){
    if(baseBlocks[codei].alive == 0){
      numTestBlocks++;
    }

    baseBlocks[codei].alive += num;
    baseBlocks[codei].code  = codei;

    nodecount += num;
  }

  // ---make sure everything is cool----
  if(error != EOF){
    printf("Really bad layout input error.\n");
    return 1;
  }

  myclose(infile);

  // create the 'minimal' array

  blocks = calloc(nodecount+1, sizeof(struct blockNode*));
  testBlocks = calloc(numTestBlocks, sizeof(struct blockNode*));
  //blocks[0] = &baseBlocks[0];

  //k = 1;
  k = 0;
  q = 0;
  for(i = 1; i < maxnodes; i++){
    if(baseBlocks[i].alive){
      testBlocks[q] = &baseBlocks[i];
      q++;

      for(j = 0; j < baseBlocks[i].alive; j++){
	blocks[k] = &baseBlocks[i];	
	k++;
      }
    }
  }

  // --- da main event ---

  printf("0 1 1\n");

  for(maxdepth = 1; maxdepth <= nodecount; maxdepth++){
    livenodes = dednodes = 0;

    iterate(maxdepth, maxnodes, nodecount);

    printf("%d %llu %llu\n", maxdepth, livenodes, livenodes+dednodes);
  }

  return 0;
}

// use pointers instead of indexes?

void iterate(int maxdepth, int maxnodes, int numnodes){
  int *Is, posi, alive, i, k, w, q, *Js, **Qs, IDAlimit, *Depths;
  code_t stuff;

  int depth = maxdepth-1;

  //  int hit = 0, miss = 0, notry = 0;

  int shadowN = N;
  counter_t shadowdednodes = 0, shadowlivenodes = 0;

  int posj, jdepth;

  struct blockNode **shadowBlocks = testBlocks;
  int testLimit = numTestBlocks;

  Is = calloc(maxdepth +1, sizeof(int));
  
  
  Qs = calloc(N, sizeof(int*));
  Depths = calloc(shadowN, sizeof(int));
  
  for(i = 0; i < N; i++){ 
    Depths[i] = -1;
    Qs[i] = calloc(numnodes +2, sizeof(int));
  }
  

  for(i = 0; i < maxdepth; i++){
    Is[i] = -1;
  }

  Is[maxdepth] = numnodes;
  posi = -1;

  while(1){
    if(posi < depth){ // depth instead of 0?
      posi = Is[depth] = Is[depth+1] - 1;
      
    }else{
      blocks[posi]->alive++;
      
      posi = --Is[depth];
      
      if(posi < depth){
        depth++;
        
        posi = Is[depth];
	
	if(depth == maxdepth){
          break;
	}else{
          continue;
        }
      }
    }
    
    blocks[posi]->alive--;    
    
    if(!(depth == 0)){
      depth--;
      posi = Is[depth];
    }else{
      // --- Test for Life ---
      alive = 1; k =0;

      for(i = 1, k = 0; i < maxnodes && alive == 1; i <<= 1, k++){
	if(baseBlocks[i].alive == 0){
	  Js = Qs[k];
	  //hit++;
	  alive = 0;
	  if(Depths[k] > 0){
	    alive = 1;
	    for(w = 1; w <= Depths[k]; w++){
	      if(!(shadowBlocks[Js[w]]->alive)){
		alive = 0; 
		//hit--;miss++;
		break;
	      }
	    }
	  }//else{
	  //  notry++;
	  // }
	  
	  if(!alive){
	    // XXX: check if its dead
	    IDAlimit = 2;
	    Depths[k] = -1;
	    
	    while(IDAlimit <= shadowN && alive == 0){
	      stuff = baseBlocks[i].code;
	      Js[0]= 0;
	      Js[1] = -1;
	      q = 1;
	      
	      while(q > 0){
		Js[q]++;
		if(Js[q] < (testLimit)){
		  if(shadowBlocks[Js[q]]->alive){
		    stuff ^= shadowBlocks[Js[q]]->code;
		    
		    if(stuff){
		      if(q < IDAlimit){
			q++;
			Js[q] = Js[q-1];
		      }else{
			stuff ^= shadowBlocks[Js[q]]->code;
		      }
		    }else{
		      // w00t, he is ALIVE!
		      alive = 1;
		      Depths[k] = q;
		      break;
		      // stuff ^= Js[q];
		    }
		  } 
		}else{
		  q--;
		  if(shadowBlocks[Js[q]]->alive){
		    stuff ^= shadowBlocks[Js[q]]->code;
		  }
		} // unless node overflow
	      } // while(q >= 0)
	      
	      IDAlimit++;
	    }// IDA while()
	    
	  }// end if blocks[i] == 0
	}// end for
      }

      if(alive == 1){
	shadowlivenodes++;
      }else{
	shadowdednodes++;
      }
    }
  }

  //printf("\t%d %d %d\n", hit, miss, notry);

  livenodes = shadowlivenodes;
  dednodes = shadowdednodes;
}

FILE *myfopen(char *filename, char *mode){
  FILE *dafile;
  char *ending, *command, *gzformat = "zcat %s";

  if(*filename == '-'){
    dafile = stdin;
  }else{
    ending = strrchr(filename, '.');

    if((ending != NULL) && (strcmp(ending, ".gz") == 0)){
      command = calloc(strlen(filename) + strlen(gzformat), sizeof(char));

      sprintf(command, gzformat);

      dafile = popen(command, mode);
    }else{
      dafile = fopen(filename, mode);
    }

    if(dafile == NULL){
      printf("Error opening '%s'!\n", filename);
    }
  }

  return dafile;
}

int myclose(FILE *dafile){
  int val = 0;

  if(dafile != stdin){
    val = pclose(dafile);

    if(val == -1){
      val = fclose(dafile);
    }
  }

  return val;
}
