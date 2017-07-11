/*
 * t_chess   originally written for a Tandy M200 80C85 based laptop
 *           adapted to run on Teensy
 *           
 *   Heuristics:   Hardly any but the program does surprising well.        
 *       Captured piece value is the main move determination.
 *       Move mobility is evaluated by counting moves available at 3rd level.
 *       Board center pieces are encouraged to move via the order of searching the board for pieces to move.
 *       The king is discouraged from moving by subbing 1 piece value when moved.( avoid check )
 *       Alternately the same code awards a piece value if causing the opponents king to move. (cause check)
 *       Bishops and Knights are encouraged to move out of the back row home base and not return via adding or 
 *          subbing a piece value during game start.
 *       Moves that limit the opposing king's number of safe moves are encouraged.
 *       In the endgame, moving the king toward the opposing king is encouraged and a check move is encouraged to a 
 *       lesser extent.
 *       Pawns are encouraged to move in the endgame. 
 *       
 *       Random play.  Avoids repeating the exact game each time.  The program does not always pick the
 *       "best" move and sometimes makes bad moves - this is considered a feature.
 *       The player can castle, but the program will not.  There are no checks that your castle move was legal.
 *       Suitable for beginner chess play.
 *       Moves are entered like d2d4, b1c3, oo, ooo
 *       
 * 
 */
 /*
 Copyright (c) 2017 Ron Carr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

typedef unsigned char uchar;  // original program was all unsigned char types
                              // some routines add value 255 to cause subtraction

struct MOVE {                 // a list of scouted moves to pick from
  uchar x;
  uchar y;
  uchar p;
  uchar q;
  uchar score;                // piececount  
  uchar movecount;            // mobility - more moves possible the next turn.
  int attacks;                // king threats - not sure this creates good moves but may open a stalled game.
  long hash;                  // sum index times piece for whole board to detect repeating positions
  int  safe;                  // how many safe moves the opposing king has if this move was made.
  int  checks;                // does this move produce a check.
  int k_distance;             // how many moves between the kings after this move
};
#define MAXMOVES 200
struct MOVE moves[MAXMOVES];

int scout;           // scouting and deeper level updating
int sindex;
int uindex;
int recording;

uchar search_depth;   // liaison between global variables and recursive routines.
uchar x1,x2,y1_,y2;   // used to check for valid player move
                      // y1 is a math function name.
int myok;             // player move valid 
int cut;              // early cutoff like alfa beta pruning
int mate;
int levels;           // index to levels to use arrays in function adjust_levels
int gsd;              // scout depth
int gdc;              // deeper search count
int gdd;              // deeper search depth
int gcut;             // allow levels routine to turn cut on and off for scout
int mobility;         // percentage of time we look at the move count for move selection
                      // higher mobility should keep the board open
int attack_;          // attack the king move pick percentage.  Not a very effective heuristic.
int avoid_stale;      // a flag that a stalemate may be chosen as a move


elapsedMillis timer;
unsigned int p_time, t_time;   // chess clock timers

#define BLACK_ 0
#define WHITE_ 8
#define PAWN   1
#define BISHOP 2
#define KNIGHT 3
#define ROOK   4
#define QUEEN  5
#define KING   6
#define P_PAWN 7
#define TRUE  255
#define FALSE  0
#define MAXLK  8
#define WIN   96
#define LOSS  160 

/* arrays for recursive routines. Index zero not used */
uchar color[MAXLK];     
uchar bestval[MAXLK];      
uchar bestx[MAXLK];
uchar besty[MAXLK];
uchar bestp[MAXLK];
uchar bestq[MAXLK];
uchar altx,alty,altp,altq;           /* alternate move for mate situations */
                             

uchar t1,t2,t3,t4; /* temp globals for general use when run out of registers ( 8085 processor ) */
uchar function;             /* searching or validation of player move */
uchar piecevalue;           /* 128 is the zero value */
uchar value[8] = { 0,2,6,6,10,18,64,2 };   /* piece values */
// uchar pawnvalue[] = {  10,5,3,2,2,2,2,0,0,2,2,2,2,3,5,10 };  // value changes depending upon row
// this seems like maybe a good idea but capture is delayed until it is worth more points, giving up pawns along the way
uchar x,y,p,q;   /* working coordinates */
uchar kingf;    /* prevent search deeper than capturing the king */
int   chkf;     /* used for checking for check */
int   attacks;    /* count of attacks on king for 1 complete move search.  Can be a large number and + or - */
                  /* counts even dumb moves, ex. the king steps in front of the opposing queen. */

/* variables for mobility evaluation */
/* count moves available for computers 2nd move - max board coverage */
/* this is used for the mobility move selection */
uchar lvlm1;
uchar lvlm2;
uchar movecnt;
uchar bestcnt;
uchar enablecnt;

// a somewhat logically challenged passed pawn implementation.
// a shadow piece is placed on the board for capture.  If the computer captures the shadow piece in its search algorithm,
// the search will continue with a pawn on the board that should not be there.  The resulting move may be weak.
uchar ppxy;       // location of the pawn that moved two spaces
uchar ppsxy;      // location of P_PAWN, the pawns shadow
int   ppactive;   // 0-not active, 1-active, 2-temp disabled for alternate uses of the search routines( ex. chkchk )
uchar p_pawn;     // a pseudo piece is placed on the board for capture only and only during pawn moves.

uchar board[64];  // one dimensional to avoid multiply on the 8085 micro processor.
int view;         // board displayed for black or for white

int turn_count;   // to alter heuristics or just to print out
char rbuf[8];     // keyboard command buffer

int debug_print = 1;   // !!! decide what to print when this is turned off


void setup() {

  Serial.begin(9600);
  delay(3000);

}


void play_game( int color ){   // game loop.  color is player color

int turn = WHITE_;
int game_over;

    mobility = random(20,80);    // variable game play
    attack_  = random(20,50);    // evaluated first so this should be lower
                                 // if attack was 50 then 50 percent moves would be attack moves and the other 50 percent
                                 // split between mobility and random pick
    Serial.println();   
    Serial.print("Mobility  ");  Serial.println(mobility);
    Serial.print("Attacking ");  Serial.println(attack_);
    while(1){
       if( turn == WHITE_ ) ++turn_count;
       if( turn == color ) game_over = player_move( turn );
       else game_over = teensy_move( turn );
       turn ^= WHITE_;
       if( game_over ) break;
    } 
}

void demo_game( int color ){   // or debug game.  computer plays all moves 

int turn = WHITE_;
int game_over;


    color = WHITE_;              // ignore the passed argument
    mobility = random(20,80);    // variable game play
    attack_  = random(20,50);
    Serial.println();   
    Serial.print("Mobility  ");  Serial.println(mobility);
    Serial.print("Attacking ");  Serial.println(attack_);
    while(1){
       if( turn == WHITE_ ) ++turn_count;
       if( turn == color ){
          draw_board();
          Serial.print("Turn "); Serial.println(turn_count);
          Serial.println("White to Move");
          view ^= WHITE_;
          game_over = teensy_move( turn );
          view ^= WHITE_;
       }
       else{
          draw_board();
          Serial.println("Black to Move");
          game_over = teensy_move( turn );
       }
       turn ^= WHITE_;
       if( turn_count > 120 ) break;
       if( game_over ) break;
    } 
}



int player_move( int color ){
char c;

  Serial.print(ppactive);
  //if( ppactive ) board[ppsxy] = p_pawn;
   print_times();
   draw_board();
   myok = 0;
   timer = 0;
   while( myok == 0 ){
      crlf();
      Serial.print("Enter Move ");
      c = get_move();
      c = tolower(c);
      if( c == 'q' ) return 1;  // quit game
      if( c == 'o' ){
         castle( color );           // no check for legal move.  Don't castle into check.
         break;
      }
      if( c == 'y' ){             // you move, computer plays a turn for the player
         view ^= WHITE_;          // scoring uses the global var view.  Kind of messy.
         teensy_move(color);   
         view ^= WHITE_;
         return mate;
      }
      tandy_main( 0, color );
      if( myok == 0 ){
        Serial.print("Invalid Move");
      }
      else if( chkchk2(color)){       // myok gets altered here
        Serial.print("Still in check or Moved into check");
        myok = 0;
      }
      else myok = 1;    
   }
   crlf();
   p_time += timer/1000;
   p_time -= 3;                     // allow 3 seconds typing time. If you type fast you can cheat the clock.
   if( c != 'o' ) move_piece( x1, y1_, x2, y2 );  // don't extra move when castle
   
   // could check for check here although the computer doesn't need a reminder
   return mate;
}

int teensy_move( int color ){
int sd;               // scout search depth
int val;              // move score
int chk;
int tmp;
int mi;
uchar fx,fy,fp,fq;

   if( ( mate = mate_detect(color)) ){   // see if we lost
       draw_board();
       Serial.print( " *** Mate ***");
       return mate;
   }

   value[PAWN] = ( turn_count > 40 ) ? 3 : 2;
   timer = 0;
   
   //  see if we are about to win and try to avoid stalemate from the deeper search
   //  pick our best level 3 move with an alternate pick move algorithm
      scout = 1;  cut = 0;  sindex = 0;         // scout at level 3, no cut as want best non stalemate move
      tmp = debug_print;           
      debug_print = 0; 
      recording = 1;   
      tandy_main( 3, color );
      recording = 0; 
      debug_print = tmp;  
      mi = pick_move2(color);                   // the move is saved in the altx, alty, altp, altq variables
      val = moves[mi].score;                    // if needed after running scout below
   print_move(3,mi); 
   
   if( val <= WIN ){                            // use this move
      fx = moves[mi].x;
      fy = moves[mi].y;
      fp = moves[mi].p;
      fq = moves[mi].q;    
   }
   else{            // run the regular scout and deeper search
      crlf();
      cut = 1;      // use cut for scout  
     // timer = 0; 
      sd = gsd;     // scout depth
      scout = 1;                             // scout a new move table
      sindex = 0;    
      move_score(0,0,0,0,0,0);                 // reset debug print margins
      recording = 1;
      val = tandy_main( sd, color );
      recording = 0;
      print_move(sd, 0);   // scout's best move and search time
      crlf();
      scout = 0;   
      sd = gdd;                             // search depth is the deeper level
      uindex = sindex;                      // in case we don't do the deeper search
      if( /* cut == 1 && */ timer < 8000 ){
          cut = gcut;                          // use cut on the deeper searches?
          uindex = 0;                       // update best moves first
          move_score(0,0,0,0,0,0);          // even up the debug display
          recording = 1;
          val = tandy_main( sd, color );    // deeper search
          recording = 0;
      }

      adjust_levels((int)timer, sindex);  // adjust search depth 
    //  print_move(sd, val );     //  get to see the new time and maybe some bogus move information
      sindex = uindex;          // cap valid moves to the ones we updated
   
      t_time += timer/1000;       // teensy's clock time.  He cheats with truncation.
      mi = pick_move(color);
      val = moves[mi].score;
      fx = moves[mi].x;            // set up to use this move
      fy = moves[mi].y;
      fp = moves[mi].p;
      fq = moves[mi].q;
    //  crlf();
      print_move(sd,mi); 
   }
    

   
   // this code block is supposed to let a known mate sequence play out to the last move
   // as opposed to calling it a win or loss from 5 moves away.  Deep levels make illegal moves when
   // a loss is coming as it just tries to limit the score damage.  Use the level 3 move when needed.
   if( val >= LOSS  || (val <= WIN && moves[mi].checks == 0 && avoid_stale )){  // search is showing a loosing mate pending,
      fx = altx;  fy = alty;             // or a stalemate.    use the default level 3 move
      fp = altp;  fq = altq;
      print_move(3,-1);                  // print this move again so the + for check ends up in the correct place
   }

   move_piece( fx,fy,fp,fq );
   
   chk = chkchk( color );
   if( chk ) bufin('+');
   crlf();
 
                                              
   if( ( mate = mate_detect(color ^ WHITE_)) ){   // see if we won
      draw_board();
      Serial.print( " *** Mate ***");
   }
   return mate;
}


 /* keep a reasonable response time, vary level and number of deeper searches */
 /* settle at a move time between 5 and 20 seconds */
 // when scouting with cut enabled, the deeper search is allowed 8 seconds in function moves_update
 // and more time if all the moves scouted have the same score
void adjust_levels( int msec, int total_moves ){
int lev_s[] = {3,3,4,4,5,5,6};     // make changes here rather than write new code when experimenting
int lev_d[] = {4,4,5,5,6,6,7};
int cuts[]  = {1,0,1,0,1,0,1};     // use cut when deeper search.  Increases speed and potential bad moves.
int level_change = 0;              // but get the benefit of a deeper search.
static int ave_time = 10000;

   gdc += 2;                             // just used to limit starting moves to central pawns

   ave_time = 7 * ave_time + msec;
   ave_time = ave_time / 8;
   crlf();
   Serial.print("Ave Time ");  Serial.println(ave_time);

   if( msec > 30000 ) level_change = -1;   // ? pawn promoted
   if( ave_time < 4000 ) level_change = 1;
   if( ave_time > 12000 ) level_change = -1;

   if( level_change ){
       levels += level_change;
       if( levels < 0 ) levels = 0;
       if( levels > 6 ) levels = 6; 
       gsd = lev_s[levels];
       gdd = lev_d[levels];
       gcut = cuts[levels];               // cut scout or not
       ave_time = 8000;                   // set to a neutral value to avoid double changes
   }
  
}



void loop() {
int color;  
char c;

   color = WHITE_;
   init_board();
   timer = 0;
   mate = 0;
   p_time = t_time = 0;
   turn_count = 0;
   levels = 1;
   gsd = 3;  gdc = 5;   gdd = 4;     // game start search levels.  gdc is 5 for center pawn moves only
   gcut = 0;

   crlf();  crlf();
   Serial.print("White or Black ? ");
   c = get_move();
   if( c == 'b' || c == 'B' ) color = BLACK_;   
   randomSeed(timer);
   view = color;
   if( c == 'd' ) demo_game(color);
   else play_game(color);

}

void print_times(){

   crlf();
   Serial.print("Turn: ");   Serial.println(turn_count);
   Serial.println("Clock:  Player     T-Chess");
   Serial.print  ("         ");
   Serial.print(p_time / 60);  Serial.print(":");
   if( (p_time % 60) < 10 ) bufin('0');
   Serial.print(p_time % 60);
   Serial.print  ("       ");
   Serial.print(t_time / 60);  Serial.print(":");
   if( (t_time % 60) < 10 ) bufin('0');
   Serial.print(t_time % 60);
   crlf();
   
}


// just debug info mostly
void print_move( int sd, int i ){

   crlf();
   //Serial.println(ppactive);

   Serial.print("Level : ");
   Serial.print(sd);
   Serial.print("     Timer ");
   Serial.println(timer);
   if( i >= 0 ){   
      Serial.print("  Value ");  Serial.print(moves[i].score); Serial.print("   Move ");
      bufin('h' - moves[i].x);
      bufin('1' + moves[i].y);
      bufin('h' - moves[i].p);
      bufin('1' + moves[i].q);
   }
   else{
      Serial.print("  Move  ");
      bufin('h' - altx);
      bufin('1' + alty);
      bufin('h' - altp);
      bufin('1' + altq);      
   }
   
   //crlf();
   
}

void update_moves( uchar x, uchar y, uchar p, uchar q, uchar val, int count, int attacks ){
// looks like we could remove most of these arguments

   if( view == BLACK_ ) val = 256 - val;
   moves[uindex].score = val;
   moves[uindex].attacks = attacks;
   moves[uindex].movecount = count;
  
}

// save all moves scouted
void save_moves( uchar x, uchar y, uchar p, uchar q, uchar val, int count, int attacks ){
int i;
int k;

   // fixup the score if computer is playing white
   if( view == BLACK_ ) val = 256 - val;  // computer is playing white

   // sort into the moves data
   for( i = 0; i < sindex; ++i ){
      if( val < moves[i].score ) break;
   }

   // this table has stale data above the value of sindex, but we don't use it ( hopefully )
   for( k = MAXMOVES - 1; k > i; --k ){
       moves[k].x = moves[k-1].x;
       moves[k].y = moves[k-1].y;
       moves[k].p = moves[k-1].p;
       moves[k].q = moves[k-1].q;
       moves[k].score = moves[k-1].score;
       moves[k].movecount = moves[k-1].movecount;
       moves[k].attacks   = moves[k-1].attacks;
   }
   moves[i].x = x;
   moves[i].y = y;
   moves[i].p = p;
   moves[i].q = q;
   moves[i].score = val;
   moves[i].movecount = count;
   moves[i].attacks = attacks;
   ++sindex;
   if( sindex == MAXMOVES ) --sindex;
}


// the main pick move function for most of the game play
int pick_move( int color ){
int i;
int j;
uchar score;
int best[6];
int mj;
int bval;
int safe;

long hash;
#define LIST_LEN  8              // the size of this list is a trade off between finding duplicate positions and false positives
static long hash_list[LIST_LEN];    // the size needs to be a power of 2 or change some code below
static int hi;
int rp;                      // value to use to alter score for duplicate position
int distance;                // the number of moves between the two kings

   // score table is from blacks viewpoint - low score is best.
   
   // alter some of the move scores if board positions are duplicated
   calc_hash(-1);                           // new hash value for the board
   safe = calc_king_safe( -1 , color );     // their king's number of current safe moves
   distance = calc_k_distance( -1, color ); // distance between the kings
   
   for( i = 0; i < sindex; ++i){            // change some scoring after the search has finished
      hash = moves[i].hash = calc_hash(i);
      rp = 1;
      for( j = 0; j < LIST_LEN; ++j ){
          if( hash == hash_list[j]  ){
              Serial.print("Hash Match "); Serial.print(rp);  // we may be seeing more false positive than expected
              Serial.print(" "); Serial.println( i );
              moves[i].score += rp;
              rp *= 2;                       // increase the penalty if see more board position repeats for this move
          }
      }
      // fill in the checks information, and safe king moves for the other side
      // don't give bonus if the move is a repeating move
      moves[i].checks = chkchk3( moves[i].x, moves[i].y, moves[i].p, moves[i].q , color );
      // get the other kings safe move count 
      moves[i].safe = calc_king_safe( i, color );
      if( moves[i].safe < safe && rp == 1 ){
        moves[i].score -= 1;                            // this move reduces the opponent king's safety.
        Serial.print("King  Safe adjust ");   Serial.println(i);
      }
      // encourage pawns to move in endgame.  Rooks also.  !!! maybe random these. Perhaps rooks is not a good idea.
      if( turn_count > 40 && (bdlkup(moves[i].x, moves[i].y) & 7) == PAWN ) moves[i].score -= 1;
      // the rook files are the last to be searched putting rooks at the end of the list for move choice.
      // too often they sit unused guarding a pawn
      //if( turn_count > 40 && (bdlkup(moves[i].x, moves[i].y) & 7) == ROOK ) moves[i].score -= 1;
      moves[i].k_distance = calc_k_distance( i, color );
      if( turn_count > 40 && moves[i].k_distance < distance && rp == 1){
          moves[i].score -= 2;                                              // king is given a greater encouragement than
          Serial.print("King Distance adjust ");  Serial.println(i);        // the check move below
                                                                            // the pawn value has been increased to 3 elsewhere
      }                                                                     // so that pawns are not sacrificed just to move
                                                                            // the king
      if( turn_count > 40 && moves[i].checks && rp == 1 ){    // check bonus in endgame
            moves[i].score -= 1;          
            Serial.print("Check Move adjust ");  Serial.println(i);
      }
    }
 
   j = 0; score = 255;   
   
   for( i = 0; i < sindex; ++i ){     // find good moves and save the index in the best array
      if( moves[i].score < score ){
          j = 0;                      // start new list of moves when we beat the current list score
          best[j++] = i;
          score = moves[i].score;
      }
      else if( moves[i].score == score  &&  j < 6 ){   // else add moves to the list.  Max 6 moves.
          best[j++] = i;  
      }
   }

   crlf();
   Serial.print("Number of moves to choose from "); Serial.println(j);
   Serial.print("  Value "); Serial.print(score);

   i = 0;                                  // best move selected as default. Index to best array.

   if( j > 1 ){                                // we have more than one move to pick from
      if( (int)random(100) < attack_ ){        // pick move based upon king attacks
         bval = -32000;                             
         for( mj = 0; mj < j; ++mj ){             // consider just the ones already selected with best score
            if( moves[best[mj]].attacks > bval ){
               bval = moves[best[mj]].attacks;
               i = mj;   
            }
         }
         Serial.print(" Attacks");   
      }   
      else if((int)random(100) < mobility ){  // pick move based upon mobility
         score = 0;                           // reuse this variable now for movecount ( type uchar )
         for( mj = 0; mj < j; ++mj ){         // consider just the ones already selected with best score
            if( moves[best[mj]].movecount > score ){
               score = moves[best[mj]].movecount;
               i = mj;   
            }
         }
         Serial.print(" Mobility");   
      }
      else{                       // pick move at random from the best moves
         i = random(j);           // random pick from up to 6 possibles
         Serial.print(" Random  ");
      }
   }
               // else j was not > 1 and we get the move with the best score
               
   i = best[i];             // get the index to the move in moves[] from the list of move indexes.

   // save the hash for this move. if in a loosing mate sequence the move may not be used. but the board
   // hash is somewhat imperfect anyway with sometime false matches resulting only in the computer perhaps
   // not making the "best" move.
   hash_list[hi++] = moves[i].hash;
   hi &= (LIST_LEN-1);      // power of 2 list length

   return i;
}


int pick_move2( int color ){   // pick the best level 3 move to use in a win scenario.  Avoid stalemate.
int i;
uchar best_score;
uchar best_check;
uchar best_win;
int bs, bc, bw;
int bb;

   avoid_stale = 0;
   for( i = 0; i < sindex; ++i ){                        //  get the checks info
      moves[i].checks = chkchk3( moves[i].x, moves[i].y, moves[i].p, moves[i].q , color );
   }

   best_score = best_check = best_win = 255;
   bs = 0;
   bc = bw = -1;

   for( i = 0; i < sindex; ++i ){
       if( moves[i].score < best_score && moves[i].score > WIN ){       // best move without stale mate
          best_score = moves[i].score;
          bs = i;
       }
       if( moves[i].checks && moves[i].score < best_check ){            // best move with a check
          best_check = moves[i].score;
          bc = i;
       }
       if( moves[i].checks && moves[i].score < best_win && moves[i].score <= WIN ){  // a winning check mate
          best_win = moves[i].score;
          bw = i;
       }
   }
   bb = 0;    // default move
   if( bw != -1 ) bb = bw;                                    // pick a winning move
   else if ( best_check <= best_score && bc != -1 ) bb = bc;  // pick a check
   else bb = bs;                                              // pick non win best move ( avoid stalemate )

   altx = moves[bb].x;             // save for possible later use
   alty = moves[bb].y;
   altp = moves[bb].p;
   altq = moves[bb].q;

   if( bw == -1 ){                                        // no winning move found, see if we need to avoid stalemate
       for( i = 0; i < sindex ; ++i ){
           if( moves[i].score <= WIN ) avoid_stale = 1;   // yes there is a "win" without a check
       }
   }
   
   return bb;
}

/*
uchar eval( uchar color ){

   return (piecevalue);   // remove this function if just piecevalue
                          // and just use piecvalue from caller
}
*/


// update the board after selections have been made
void move_piece( uchar x, uchar y, uchar p, uchar q ){
int i,j;
int dy;
uchar piece;
uchar c;

    i = 8*y + x;   j = 8*q + p;
    i &= 63;  j &= 63;             // force in bounds

    if( ppactive == 1){                        // pp active last move and captured ?
       c = p_pawn & WHITE_;
       c ^= WHITE_;                           // color of capturing pawn
       if( board[i] == (PAWN + c) ){          // it was a pawn move of the appropriate color
           if( j == ppsxy ){                  // the pawn moved to the capture shadow spot
              board[ppxy] = 0;                // remove the pawn that jumped two spaces from the board
              if( debug_print ) Serial.println("Passed Pawn Capture");
           }
       }        
    }
    
    board[j] = board[i];                      // move piece
    board[i] = 0;
    promote2();

    // deactivate and maybe activate passed pawn 
    ppactive = 0;
    piece = bdlkup(p,q);
    if( ( piece & 7 ) != PAWN ) return;
    dy = (int)q - (int)y;
    if( dy == 2 || dy == -2 ){   // last move was pawn and 2 space move
       dy *= 4;                  // 8 or -8  delta board index
       ppactive = 1;
       ppxy = j;        // location of pawn
       ppsxy = j - dy;  // one space behind the pawn for location of capture
       p_pawn = P_PAWN + (piece & WHITE_);   // get correct color pseudo pawn capture shadow piece
    }
    
}

// read from serial until we get a (carriage return) line feed
char get_move(){
char c;
int i;

  Serial.print( "> " );
  i = 0;
  while( 1 ){
    if( Serial.available() ){
       c = rbuf[i++] = Serial.read();
       i &= 7;
       if( c == '\n' || c == '\r' ) break;
    }
  }
    
  x1  = 'h' - rbuf[0];  // decode move to x and y
  y1_ = rbuf[1] - '1';
  x2  = 'h' - rbuf[2];
  y2  = rbuf[3] - '1';

    rbuf[ i-1 ] = 0;
    Serial.print( rbuf );  Serial.print( "  " );
   
  return rbuf[0];   // return a one letter command. quit, castle, black, white, you move, demo game.

}
//  mirror image, 0,0 is the lower right corner.
void init_board(){
int i;
const uchar backrow[] = { ROOK,KNIGHT,BISHOP,KING,QUEEN,BISHOP,KNIGHT,ROOK };

   for( i = 0; i < 8; ++i ){
      board[i] = backrow[i] + WHITE_;
      board[i+8] = PAWN + WHITE_;
      board[i+48] = PAWN;
      board[i+56] = backrow[i];
   }
   for(i= 16; i < 48; ++i ) board[i]= 0;
}

void move_score(uchar x, uchar y, uchar p, uchar q, uchar val, int bnty ){  // debug function
static int count;

   if( val == 0 ){
      count = 0;
      if( cut ) Serial.println("cut");
      return;
   }

   if( debug_print == 0 ) return;

      // fixup the score if computer is playing white
   if( view == BLACK_ ) val = 256 - val;  // computer is playing white
   
   bufin('h' - x);
   bufin('1' + y);
   bufin('h' - p);
   bufin('1' + q);
   bufin(' ');
   Serial.print(val);
 //  if( bnty >=0 )bufin(' ');
 //  Serial.print(bnty);
   bufin(' ');
   if( ++count > 5 ) count = 0, crlf();
}

 /*
          8 7 6               0 1 2
          5 4 3               3 4 5
          2 1 0 white view    6 7 8  black view 

          a b c               c b a   */

 /* convert from letter  to index  is  'h' - letter */
 /* convert from index to letter is 'h' - index(mod 8)  */
void draw_board(){   
int t,i,p,q,x,y;
const char pcs[] = {'%','P','B','N','R','Q','K','*',' ','d','b','n','r','q','k','*'}; 
                  // % is black square        // * is p_pawn but it is not supposed to ever show

   t= WHITE_;   /* toggle for black white empty checker board */

   i = 0;     /* setup for black view */
   p= q= 1;
 
   if( view == WHITE_ ) {
     i= 63;       /* start square */
     q= -1;     /* inc value */
     p= 8;        /* start row */
     }
   crlf();
   for( x= 0; x < 64; ++x ){ 
     y = board[i];
     if( y == 0 ) y= t;    /* = toggle for empty square color */
     bufin(' ');
     bufin(pcs[y]);
     t^= WHITE_;
     i+= q;
     if( ( x & 7 ) == 7 ){  /* end of this row */
        bufin(' '); bufin(' ');
        bufin(p + '0');
        crlf();
        t^= WHITE_;
        p+= q;
        }
     } /* end x loop */

   crlf();

   i= 'a'; q= 1;
   if (view == BLACK_){  
      i= 'h'; q= -1;
      }

   for(x= 0; x < 8; ++x ){   /* print column letters */
      bufin(' ');
      bufin(i);
      i+= q;
      }

   crlf();
   // crlf();

}

void crlf( ){
  Serial.println();
}

void bufin( char c ){
  Serial.print(c);
}


/*  chess.co  companion program to m200 chess.ba */
// alot of the following was the code that ran on the Radio Shack M200 laptop
// and has been changed where needed, enhanced and mutilated.



int tandy_main(int depth, int play_color){       /*  */
int val = 0;


   /* move checker */
   if( depth == 0 ){   
      search_depth = function= 0;
      val = validate(play_color);
      if( ppactive == 1 && (board[ppsxy]&7) == P_PAWN ) board[ppsxy] = 0;      
      return val;
   }
 
   search_depth = depth & 7;              /* max seven levels */
   for(t2= search_depth; ; --t2){         /* set up alternate colors */
      color[t2] = play_color;
      if( t2 == 0 ) break;    /* unsigned test only */
      play_color = play_color ^ WHITE_;
      }

   function= 2;
   piecevalue= 128;
   bestcnt= kingf= 0;
   /* precalc these values outside recursive loops */
   lvlm1= search_depth - 1;   /* stop count level */
   lvlm2= search_depth - 2;   /* count level */
   
   if( scout ) val = minmax(search_depth);
   else val = moves_update(search_depth);
         
   if( ppactive == 1 && (board[ppsxy]&7) == P_PAWN ) board[ppsxy] = 0;   // needed here also for computer moves?
 
 
   return val;
   
}   /* end main */

// look at scout moves and search deeper
int moves_update( int depth ){
int i;  
uchar dv;   // will need to figure out what the stop should be for pawn moves
int base_score;

  if( ppactive ) ++ppactive;     // disable
  base_score = moves[0].score;   // sorted values with best first

  // the moves data is updated from the move_() recursive function using the uindex variable.  This is just a driver
  // function to do the selected move first.
  for( i = 0; i < sindex; ++i ){
      if( i >= gdc ) break;                 // limit moves when starting
      if( i >= 1 ){                         // need at least one move or everything stops
         if( (int)timer > 8000 && moves[i].score > base_score ) break;   // do as many as can in 8 seconds
         if( (int)timer > 20000 ) break;                                // but do all the tied values up to 20 seconds
         if( moves[i].score >= LOSS ) break;     // don't waste time.  This move already a loss mate and can't change it
                                                 // and they are sorted so the rest will be also
         if( moves[i].score <=  WIN ) break;     // same for a win mate although there may be better choice due to this
                                                 // part of the program detects stalemate as a mate
         if( moves[i-1].score <= WIN ) break;    // deeper search just found a mate with the last move
      }
      function= 2;                       // need to re-initialize for each deeper search
      piecevalue= 128;                   // fix for cut issues ?
      bestcnt= kingf= 0;        // if kingf was stuck on, then the search was not really done.
      bestval[depth]= ( color[depth] == WHITE_ ) ? 1 : 255;  // this was the stale data cut was using ?  

      x = moves[i].x;                     // set up the move from the scout info
      y = moves[i].y;
      p = moves[i].p;
      q = moves[i].q;
      dv = 2;                             // normal stops
      if( (bdlkup(x,y) & 7) == PAWN ){
         dv = ( x == p ) ? 0 : 1;         // pawn normal and capture stops.  different column(x) is capture,
      }
      move_( depth, dv );                 // skip the first level of minmax and make this move before going
      ++uindex;                           // down the recursion tubes.  Point to next move in table.
  }

  if( ppactive ) --ppactive;
  return bestval[depth];   // not meaningful here,  is there something else to return?
}


inline uchar bdlkup(uchar col, uchar row ){  /* lookup piece on board */
                                    // inline makes no difference in times ?  
   return board[8*row + col];       // maybe make this a macro
}


/*  note: dv was originally used to force integer alignment of the
 *   function arguments for the 80c85. All auto class variables where held in registers.  
 *   As you remember there are b,c,d,e,h,l registers and bc,de,hl pairs.
 */
int validate(int player){    // test the user made a legal move
uchar dv;
uchar cpiece;  /* piece with color */

   dv = 0;
   x= x1;   y= y1_; 
   if( x > 7 || y > 7 ) return FALSE;                        // maybe mistyped
   myok= FALSE;                                              // assume he made a mistake
   color[0]= (cpiece= bdlkup(x,y)) & WHITE_;                 // require he moves the correct color
   if( player == color[0]  && cpiece ) movegen(dv,cpiece);   // level 0 search only
   
   return myok;
}



// call a tied for piececount a better move if moving a piece out of the backrow
// but not before a couple of pawns have moved
// this should get the knights and bishops into play
int backrowck( uchar x, uchar y, uchar p, uchar q, uchar color){
uchar pc;
int c;
int j,k;

    if( turn_count > 35 ) return 0;   // turn this off at some point
 // complete re-write as we also want to discourage piece moving back to home base
 // as well as moving out
    c = 0;
    k = ( color == BLACK_ ) ? 6 : 1;     // check if at beginning of the game, count pawns
    for( j = 0; j < 8; ++j ){
       if( (bdlkup( j,k ) & 7) == PAWN ) ++c;
    }
    if( c > 7 ) return 0;               // require a pawn move first

    c = 0;
    k = ( color == BLACK_ ) ? 7 : 0;    // row 0 or row 7
    pc = bdlkup( x,y ) & 7;
    
    if( y == k ){                       // moving out 
         if( pc == KING || pc == ROOK ) ;   // ignore 
         else ++c;                          // piece moving out. Good.
    }
    if( q == k ){                       // moving in
         if( pc == KING || pc == ROOK ) ;    // ignore
         else --c;
    }
    if( color == BLACK_ ) c = -c;      // black plays min.  white is max
    
    return c;
  
}

/* recursive functions, minmax() movegen() 6pieces() move()  */
/* recursive functions need to have all info needed to be saved on
   stack in the b c d e registers - 1st 4 uchar arguments */
uchar minmax(uchar depth){   
uchar t1;
uchar i;
uchar cd;  /* color of depth in register var */

   if( depth == 0 || kingf ) return piecevalue;    //eval(color[depth]);   
   cd= color[depth];
   bestval[depth]= ( cd == WHITE_ ) ? 1 : 255;

   /* find each piece and generate moves */
   if( ppactive == 1 && (board[ppsxy]&7) == P_PAWN ) board[ppsxy] = 0;   // remove passed pawn capture shadow
   x= 4;
   for( i= 0; i < 8; ++i ){
      x= (i & 1) ? x - i: x + i;  /* gen 43526170 seq, encourage center piece moves */
      for( y= 0; y < 8; ++y ){
         if( (t1= bdlkup(x,y)) ) {    /* assignment wanted */
            if( (t1 & WHITE_) == cd ){
               movegen(depth,t1);
         /* cutoff */
         /* this produces bad moves sometimes, if using scout with cut enabled */
         /* or added heuristics are altering the result */
              if( cut ){  
               if(depth != search_depth){
                  t1= bestval[depth];
                  if ( cd == WHITE_ ){
                     if( bestval[depth + 1] < t1-1 ) return t1;   //  was 1 pawn values margin -2
                     }
                  else {
                     if( bestval[depth + 1] > t1+1 ) return t1;   // was t1 + 2
                     }
               } 
              }/* cutoff */    

            }  /* correct color */
         } /* end if piece */
      }  /* end for y */
   }  /* end for i */

   return bestval[depth];     
}


/* need separate movegen function for valid move checker call */
void movegen(uchar depth, uchar cpiece ){

   switch( cpiece & 7 ){ 
      case PAWN:    pawn(depth); break;
      case BISHOP:  bishop(depth); break;
      case KNIGHT:  knight(depth); break;
      case ROOK:    rook(depth);  break;
      case QUEEN:   rook(depth);  bishop(depth); break;
      case KING:    king(depth); break;
   }

}


/* returns 0 if empty and inbounds */
/*         1 if has other color piece */
/*         2 if has same color piece or out of bounds */
/*         special pawn flag, match flag if not 2 */
uchar bounds(uchar depth,uchar pawnf){
uchar t1;
uchar t2;

   /* check if off board */
  /* if( p > 7 || q > 7 ) return 2;  */
   if( ( p | q ) & 0xf8 ) return 2;     /* check for 8 or -1 */
   /* get piece and color if any */
   t2= (t1= bdlkup(p,q)) & WHITE_;
   t1= t1 & 7;
   /* new algorithm for speed */
   t2= color[depth] ^ t2;   /* 0 == same color, 8 == other color */
   if( pawnf == 1 ){     /* pawn capture test */
      if( ( t1 + t2 ) > 8 ) return 1;    /* other color piece is there */
      return 2;                          /* blocked by empty or same color */
      }
   if( t1 == 0 ) return 0;      /* no blocks */
   /* have a piece of some color in the way */
   if(pawnf == 0 ) return 2;     /* pawn blocked ahead by piece */
   if( t2 == 8 ) return 1;       /* capture other color */
   return 2;                     /* blocked by same color */

}


uchar move_(uchar depth,uchar msave){    /*  move piece */
uchar mxy;          /* save globals on stack with these local vars */ 
uchar mpq;

int promoted;
uchar pc;

/*
   msave used for multiple stack storage
   pawn flag, stop condition, piece taken storage 
*/
   promoted = 0;                 // nothing promoted yet
   
   msave= bounds(depth,msave);  /* msave doubles as pawn flag */
   if( msave == 2 ) return 2;   /* stop */
   msave = msave << 4;          /* save stop in upper nibble */

   switch( function ){
   case 0:     /* just check valid move */
      if( p == x2 && q == y2 ) myok= TRUE;
      break;

   default:
      /* move piece */

      if(depth == search_depth){   /* count moves one branch of tree */
         movecnt= 0;
         enablecnt= 1;
         }
      /* count computer player moves one branch of tree */
      if(enablecnt && depth == lvlm2 ) movecnt= movecnt + 1;

      msave= bdlkup(p,q) | msave;   /* piece taken */
 /* xy pq merged into index to board array, saved in register var on stack */
      mxy= ((y << 3 ) | x ) ;
      mpq= ((q << 3 ) | p ) ;
      
   if( depth == search_depth ) attacks = 0;  // how many checks if we make this move, looking at all dumb moves
   
      movef(depth,msave,mxy,mpq);  /* uses t2-t4 */

      pc = board[mpq];                                    // promote pawn to queen
      if( (pc & 7) == PAWN  && (mpq < 8  || mpq > 55) ){
          promoted = 1;
          board[mpq] +=  4;    
      }
      
      t1= minmax(depth - 1 );    /* recursive call */
      
      if( promoted ) board[mpq] -= 4;     // queen back to a pawn
      moveb(depth,msave,mxy,mpq);  /* uses t2-t4 */

      /* detect best moves */
      if( depth == lvlm1 ) enablecnt= 0; /* counted one branch */
      t2= 0;
      t3= bestval[depth];
      t4= color[depth];

      if( depth == search_depth ) t1 += backrowck(x,y,p,q,t4);  // encourage getting pieces into play
      if( t4 == WHITE_  && t1 > t3 ) t2= 1;   /* best for white */
      if( t4 == BLACK_  && t1 < t3 ) t2= 1;   /* best for black */

      if( depth == search_depth ){
          if( recording /*depth >= 3 */ ){         // recording
             move_score(x,y,p,q,t1,attacks);   // debug display
             if( scout ) save_moves(x,y,p,q,t1,movecnt,attacks);
             else update_moves(x,y,p,q,t1,movecnt,attacks);
          }
      }
          
      if( t2 ){
         bestval[depth] = t1;
         bestx[depth] = x;
         besty[depth] = y;
         bestp[depth] = p;
         bestq[depth] = q;
      }
      break;
   } /* end switch */

   return msave >> 4;   // return stop saved in upper nibble

}

void movef(uchar depth,uchar msave,uchar mxy,uchar mpq){

      t2= msave & 0xf;     /* piece taken */
      t3= t2 & WHITE_;
     // if( ( t2 & 7 ) == PAWN ) t4 = pawnvalue[t2 + (mpq >> 3)]; // value changes as row changes.  Else
      t4= value[t2 & 0x7];
      if( (t2 & 7) == KING ){    // the king was captured.  Set some flags.
        kingf = 1, chkf = 1;
       // if( depth == search_depth - 2 ){        // look at attacks at the end of the search
           if( color[depth] == color[search_depth] ) ++attacks;  else --attacks;
       // }
      }
      msave= board[mxy];    /* piece moving, msave used below */
      board[mpq]= msave;
      board[mxy]= 0;
    /* adjust piecevalue for piece taken, add/subs zero if land on empty space */
      if(t3) piecevalue= piecevalue - t4;     // t3 is color
      else piecevalue= piecevalue + t4;
    /* prevent king from taking off, or get point for bothering king */
      if( turn_count < 40 && (msave & KING) == KING ){
         if( (msave & WHITE_) == WHITE_ )piecevalue= piecevalue - 1;
         else piecevalue= piecevalue + 1;
      }
    /* Move pawns down the board in endgame but we don't know why. just moving the piece is worth points */
    // try something for pawns in pick_move instead of the search algorithm.  This might be a bit too strong of
    // encouragement to move a pawn
  //    if( depth == search_depth && turn_count > 40 && (msave & PAWN) == PAWN ){
  //       if( (msave & WHITE_) == WHITE_ )piecevalue= piecevalue + 1;
  //       else piecevalue= piecevalue - 1;
  //    }

      
}

void moveb(uchar depth,uchar msave,uchar mxy,uchar mpq){

      t2= msave & 0xf;
      t3= t2 & WHITE_;
      // if( ( t2 & 7 ) == PAWN ) t4 = pawnvalue[t2 + (mpq >> 3)];
      t4= value[ t2 & 0x7 ];
      if( (t2 & 7) == KING ) kingf= 0;
      x= ( mxy ) & 0x7;
      y= mxy >> 3;
      p= ( mpq ) & 0x7;
      q= mpq >> 3;
      msave= board[mpq];         /* re-use variable */
      board[mxy] = msave;
      board[mpq] = t2;
      if(t3) piecevalue = piecevalue + t4;
      else piecevalue = piecevalue - t4;
    /* prevent king from taking off */
      if(  turn_count < 40 && (msave & KING) == KING ){
         if( (msave & WHITE_) == WHITE_ )piecevalue= piecevalue + 1;
         else piecevalue= piecevalue - 1;
      }
    /* Move pawns down the board in endgame but we don't know why. just moving the piece is worth points */
    //  if( depth == search_depth && turn_count > 40 && (msave & PAWN) == PAWN ){
    //     if( (msave & WHITE_) == WHITE_ )piecevalue= piecevalue - 1;
    //     else piecevalue= piecevalue + 1;
    //  }
      

}


void rook( uchar depth ){
uchar dv = 0;

    qbrmove(depth,dv,1,0);
    qbrmove(depth,dv,255,0);
    qbrmove(depth,dv,0,1);
    qbrmove(depth,dv,0,255);
}

void bishop( uchar depth ){
uchar dv = 0;

    qbrmove(depth,dv,1,1);
    qbrmove(depth,dv,1,255);
    qbrmove(depth,dv,255,1);
    qbrmove(depth,dv,255,255);
}

void qbrmove( uchar depth, uchar dv, uchar dx, uchar dy ){

   p= x;  q= y;  dv= 2;  /* dv used for stop */
   do{
      p= p + dx;
      q= q + dy;
      } while( move_( depth,dv ) == 0 );
}


void knight(uchar depth){
uchar dv;
uchar dy;
uchar inc;

   dv= 2; p= x - 3;   dy=0;  inc= 1;
   while( p != x + 2 ){
      dy= dy + inc;      
      p= p + 1;
      q= y + dy;
      if( p == x ){     /* zero coor */
         inc= 255;      /* -1 */
         continue;
      }
      move_(depth,dv);
      q= y - dy;
      move_(depth,dv);
   }
}


void king(uchar depth){
uchar sa,sb;
  sa= x+2;                        /* less than doesn't always work */
  sb= y+2;                        /* for these unsigned uchars */
  for( p= x-1; p != sa ; ++p ){   /* so using != to stop the loops */ 
     for( q= y-1; q != sb; ++q ){
        move_(depth,2); 
     }
  }  
}


void pawn(uchar depth){      /* pawn is only piece that stalls moving ahead */
uchar pawnf;
uchar stop_;

   stop_ = 0;
   pawnf = 1;  /* test captures first */
   p= x + 1;   q= y;
   if( ppactive == 1 && depth == search_depth  ) board[ppsxy] = p_pawn; // allow capture of the passing pawn shadow
                                                                  // remove before next level is searched as valid for
   if( color[depth] == WHITE_ ){                                  // only 1st move.
      q= q + 1;   /* up */
      move_(depth,pawnf);     /* capture right */
      p= p - 2;
      move_(depth,pawnf);     /* capture left */ 
      p= x;   pawnf = 0;
      stop_ = move_(depth,pawnf);
      if( stop_ == 0 && y == 1 ){
         q= q + 1;   
         move_(depth,pawnf);
      }
   }
   else{    /* black */
      q= q - 1;   /* down */
      move_(depth,pawnf);     /* capture  */
      p= p - 2;
      move_(depth,pawnf);     /* capture  */
      p= x;   pawnf = 0;
      stop_ = move_(depth,pawnf);
      if( stop_ == 0 && y == 6){
         q= q - 1;
         move_(depth,pawnf);
      }
   }
}


void promote2( ){   // after the move promotion. Not undone. Not part of the search moves algorithm.
int i;

   for( i = 0; i < 8; ++i ){
      if( board[i] == PAWN ) board[i] += 4;
      if( board[i+56] == (PAWN + WHITE_) ) board[i+56] += 4;
   }
}


int mate_detect( int color ){    // 2 level search to see if the king is captured
uchar val;
int mate;

    mate = 0;
    cut = 0; scout = 1;
    if( ppactive ) ++ppactive;   // ignoring passed pawn moves for now.
    val = tandy_main( 2, color );
    if( val <= WIN || val >= LOSS ) mate = 1;   // win/loss reversed for white, but don't think we care here.
    if( ppactive ) --ppactive;
    return mate;
}

/* make double moves to find check status */
int chkchk(int color){  

      chkf= 0;
      cut = 0;
      scout = 1;
      if( ppactive ) ++ppactive;
      
      tandy_main( 1 , color );      /* same color moves again */

      if( ppactive ) --ppactive;
      return chkf;
   
}


int chkchk2( int color ){   /* see if player is still in check after his proposed move */

uchar piece;
int pbs, pbd;

  /* make the players move on the board and save piece taken if any */
      chkf= 0;
      cut = 0;
      scout = 1;
      if( ppactive ) ++ppactive;   // temp disable
      
      // proposed move is in some global variables, change to board index
      pbs = 8 * y1_ + x1;
      pbd = 8 * y2  + x2;
      
      piece= board[pbd];           // player moves
      board[pbd]= board[pbs];
      board[pbs]= 0;

      color ^= WHITE_;              // change color to computers move
                                    // then run the computers next possible moves
      tandy_main(1, color);         // run one move only
      
      // move back the temp move
 
      board[pbs]= board[pbd];
      board[pbd]= piece;

      if( ppactive ) --ppactive; 

   
   return chkf;                     // gets set if the king has been captured
}

// see if the computers proposed move results in a check
int chkchk3( int x, int y, int p, int q, int color ){
uchar piece;
int pbs, pbd;

      chkf= 0;
      cut = 0;
      scout = 1;
      if( ppactive ) ++ppactive;   // disable for this routine
      
      // change to board index
      pbs = 8 * y + x;
      pbd = 8 * q + p;
      
      piece= board[pbd];           // computer moves
      board[pbd]= board[pbs];
      board[pbs]= 0;

      tandy_main(1, color);         // run one move only
      
      // move back the temp move
      board[pbs]= board[pbd];
      board[pbd]= piece;

      if( ppactive ) --ppactive;
   
      return chkf;
}

void castle(int player){     /* oo or ooo, o-o, o-o-o no check for a valid move, player only move */
int kingside;

   kingside = 0;
   if( rbuf[1] == '-' ){    // command is form o-o
      if( rbuf[3] != '-' ) kingside = 1;
   }
   else{
      if( rbuf[2] != 'o' && rbuf[2] != 'O' ) kingside = 1;
    
   }
   if(kingside){
      if( player == WHITE_ ) x= 3,y= 1,p= 0,q= 2;
      else x= 59,y=57,p= 56,q=58;
      }
   else{
      if( player == WHITE_ ) x= 3,y= 5,p= 7,q= 4;
      else x= 59,y= 61,p= 63, q= 60;
   } 

   board[y]= board[x];      
   board[q]= board[p];
   board[x]= 0;
   board[p]= 0;

}

long calc_hash( int m ){   // part of avoiding repeat board positions
static long board_hash;    // the hash is the sum of all the board pieces * the board index
                           // hopefully that will be somewhat unique.  Probably would fit in an int type.
long hash= 0;                           
int i;
int p;

    if( m == -1 ){         // just update our local board_hash
        board_hash = 0;
        for( i = 0; i < 64; ++i ){
           board_hash += i * board[i];
        }
    }
    else{                  // return the hash for the proposed move
        hash = board_hash;
        i = 8 * moves[m].y + moves[m].x;   // sub out the moving piece
        p = board[i];
        hash -= i * p;
        i = 8 * moves[m].q + moves[m].p;   // sub out any captured piece
        hash -= i * board[i];
        hash += i * p;                     // add in the moving piece at its new location        
    }
    return hash;   
}

// how many safe moves does the opposing king have
int calc_king_safe( int m, int color_k ){
int s_moves;                            // invalid types int[int] error if have two variables of the same name
int i;                                  // and try to de-reference a scalar as an array.  Why not just say you
                                        // are de-referencing a scalar as an array.
uchar piece;
int pbs, pbd;

   color_k ^= WHITE_;                   // we will be moving the other colors king
   for( i = 0; i < 64; ++i ){           // find the other color king on the board
      if( board[i] ==  KING + color_k ) break;
   }
   if ( i == 64 ) return 0;             // the king is missing?   Flakey Puffs.
   x = i % 8;  y = i / 8;               // set the global vars for the kings position.
   color[2] = color[0] = color_k;       // set the color array
   color[1] = color_k ^ WHITE_;
   
   s_moves = 0;
   if( m == -1 ){                        // how many safe moves does the other king have now?
      s_moves = king_safe( (uchar)2 );     // 2 level, I move the king, you capture me.
   }
   else{                                 // how many safe moves after making this proposed move.
      pbs = 8 * moves[m].y + moves[m].x;
      pbd = 8 * moves[m].q + moves[m].p;
      
      piece= board[pbd];           // computer moves
      board[pbd]= board[pbs];
      board[pbs]= 0;

      s_moves = king_safe( (uchar)2 );   // count the king's possible moves

      board[pbs]= board[pbd];         // move back the temp move
      board[pbd]= piece;
   }

   return s_moves;
}

// move generator to find how many safe moves the opposing king has
int king_safe( uchar depth ){
uchar sa,sb;
int count;
uchar stop_;

  if( ppactive ) ++ppactive;           // turn off passed pawn for this search
  cut = 0;
  search_depth = depth;     // correct global depth as we are bypassing tandy_main
  scout = 1;
  count = 0;
  sa= x+2;
  sb= y+2;
  for( p= x-1; p != sa ; ++p ){
     for( q= y-1; q != sb; ++q ){
        chkf = 0;
        stop_ = move_(depth,2);
        if( chkf == 0 && stop_ != 2 ) ++count;
     }
  }
//  Serial.print("King Safe Moves ");  Serial.println(count);
  if( ppactive ) --ppactive;
  return count;  
}

// the idea about king distance is in the endgame, move your king toward the opponent, if that is not possible
// then check
int calc_k_distance(int mi, int color ){

static int kx, ky;    // the opposing kings location.  saved so we don't have to find it over and over
int dx,dy;
int x,y;
int i;
int dist;

   x = y = 0;         // remove compiler warning
   if( mi == -1 ){    // find the current distance
      for( i = 0; i < 64; ++i ){
          if( (board[i] & KING) == KING ){
              if( (board[i] & WHITE_) == color ) x = i % 8 , y = i / 8;   // my king
              else kx = i % 8 , ky = i / 8;                               // their king
          }
      }
      dx = x - kx;  dy = y - ky;    
   }
   else{              // find the distance if this move is made
      i = 8 * moves[mi].y + moves[mi].x;
      if( (board[i] & KING) != KING ) return 42;    // not a king move, return a large number that discredits this move
      dx = moves[mi].p - kx;   dy = moves[mi].q - ky;   // the new location is p and q.
   }

   if( dx < 0 ) dx = -dx;
   if( dy < 0 ) dy = -dy;

     // the distance is the larger of dx dy and this accounts for diagonal moves
     // !!! maybe add one diagonal extra to detect when kings are directly opposing ( dx not zero or dy not zero )
     // !!! although that will not work well for trapping the king in the corner
   //return max( dx, dy );
   dist = max( dx, dy );
   if( dx != 0 && dy != 0 ) dist += 1;
   return dist;
}

/******************************************************************************************************/

