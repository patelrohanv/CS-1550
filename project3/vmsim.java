import java.io.*;
import java.util.*;

public class vmsim{
     static int[] frames;
     static Hashtable<Integer, pte_32> pageTable;
     public static void main (String[] args){
          //./vmsim –n <numframes> -a <opt|clock|aging|work> [-r <refresh>] [-t <tau>] <tracefile>
          int numFrames = -1;
          int refresh = -1;
          int tau = -1;
          int traceFileIndex = args.length-1;
          String tracefile = args[traceFileIndex];
          String algorithm = "";
          for(int i = 0; i < traceFileIndex; i+=2){
               int j = i+1;
               if(args[i].equals("-n")){
                    numFrames = refresh = Integer.parseInt(args[j]);
                    frames = new int[numFrames];
               }
               else if(args[i].equals("-a")){
                    algorithm = args[j];
               }
               else if(args[i].equals("-r")){
                    refresh = Integer.parseInt(args[j]);
               }
               else if(args[i].equals("-t")){
                    tau = Integer.parseInt(args[j]);
               }
          }
          //only for testing
          // System.out.println(numFrames);
          // System.out.println(algorithm);
          // System.out.println(refresh);
          // System.out.println(tau);

          //Initialize page table
          pageTable = new Hashtable<Integer, pte_32>();
          for(int i = 0; i < 1048576; i++){ //start for loop
               pageTable.put(i, new pte_32(0, false, false, false, -1));
          } //end for loop

          //call algorithms
          if(algorithm.equals("opt")){
               opt(tracefile);
          }
          else if(algorithm.equals("clock")){
               clock(tracefile);
          }
          else if(algorithm.equals("aging")){
               aging(refresh, tracefile);
          }
          else if(algorithm.equals("work")){
               workSetClock(tau, refresh, tracefile);
          }

     }
     public static void opt(String tracefile){
          //Simulate what the optimal page replacement algorithm would choose if it had perfect knowledge
          int memoryAccesses = 0;
          int pageFaults = 0;
          int diskWrites = 0;
          try{
               Scanner inFile = new Scanner(new File(tracefile));

               Hashtable<Integer, LinkedList<Integer>> expected = new Hashtable<Integer, LinkedList<Integer>>();

               for(int i = 0; i < 1048576; i++){ //start for loop
                    expected.put(i, new LinkedList<Integer>());
               } //end for loop

               int refTime = 0; //reference time
               while (inFile.hasNext()) { //start while loop
                 String line = inFile.nextLine();
                 int pageNumber = Integer.decode("0x" + line.substring(0, 5));

                 expected.get(pageNumber).add(refTime); //add the reference time to the linked list mapped a memory location
                 refTime++;
               } // end while loop
               inFile.close();
               //start opt
               inFile = new Scanner(new File(tracefile));
               int framePointer = 0;
               while (inFile.hasNext()) { //start while loop
                      String line = inFile.nextLine();
                      int pageNumber = Integer.decode("0x" + line.substring(0, 5));
                      char readWrite = line.toCharArray()[line.length()-1];
                      expected.get(pageNumber).removeFirst();
                      pte_32 pageEntry = pageTable.get(pageNumber);
                      pageEntry.presentLocation = pageNumber;
                      pageEntry.referenced = true;
                      if(readWrite == 'W'){
                           pageEntry.dirty = true;
                      }
                      if(pageEntry.valid){
                           //no page fault
                           pageTable.put(pageNumber, pageEntry);
                           System.out.println("hit: " + line);
                           memoryAccesses++;
                           continue;
                      }
                      //page fault
                      pageFaults++;
                      if(framePointer < frames.length){
                           frames[framePointer] = pageNumber;
                           pageEntry.physicalAddress = framePointer;
                           pageEntry.valid = true;
                           framePointer++;
                           System.out.println("page fault - no eviction: " + line);
                      }
                      else if(framePointer >= frames.length){
                           //start opt
                           int distance = 0;
                           int max = 0;
                           for (int i = 0; i < frames.length; i++){//start for
                                if(expected.get(frames[i]).isEmpty()){
                                     distance = frames[i];
                                     break;
                                }
                                if(expected.get(frames[i]).get(0) > max){
                                     max = expected.get(frames[i]).get(0);
                                     distance = frames[i];
                                }
                           }//end for
                           //end opt

                           pte_32 temp = pageTable.get(distance);
                           if(!temp.dirty){
                                System.out.println("page fault - eviction clean: " + line);
                           }
                           else if(temp.dirty){
                                System.out.println("page fault - eviction dirty: " + line);
                                diskWrites++;
                           }
                           frames[temp.physicalAddress] = pageEntry.presentLocation;
                           pageEntry.physicalAddress = temp.physicalAddress;
                           pageEntry.valid = true;
                           temp.setBooleanFalse();
                           temp.resetPhysicalAddress();
                           pageTable.put(distance, temp);
                      }
                      pageTable.put(pageNumber, pageEntry);
                      memoryAccesses++;
               } //end while loop
               inFile.close();

               System.out.println("Algorithm: Opt");
               System.out.println("Number of Frames: " + frames.length);
               System.out.println("Total memory accesses: " + memoryAccesses);
               System.out.println("Total page faults: " + pageFaults);
               System.out.println("Total writes to disk: " + diskWrites);
          }
          catch(FileNotFoundException e){

          }
     }
     public static void clock(String tracefile){
          //Use the better implementation of the second-chance algorithm
          int memoryAccesses = 0;
          int pageFaults = 0;
          int diskWrites = 0;
          try{
               Scanner inFile = new Scanner(new File(tracefile));

               int clockPointer = 0; //reference time
               int framePointer = 0;
               while (inFile.hasNext()) { //start while loop
                      String line = inFile.nextLine();
                      int pageNumber = Integer.decode("0x" + line.substring(0, 5));
                      char readWrite = line.toCharArray()[line.length()-1];
                      pte_32 pageEntry = pageTable.get(pageNumber);
                      pageEntry.presentLocation = pageNumber;
                      pageEntry.referenced = true;
                      if(readWrite == 'W'){
                           pageEntry.dirty = true;
                      }
                      if(pageEntry.valid){
                           //no page fault
                           pageTable.put(pageNumber, pageEntry);
                           memoryAccesses++;
                           System.out.println("hit: " + line);
                           continue;
                      }
                      //page fault
                      pageFaults++;
                      if(framePointer < frames.length){
                            System.out.println("page fault - no eviction: " + line);
                           frames[framePointer] = pageNumber;
                           pageEntry.physicalAddress = framePointer;
                           pageEntry.valid = true;
                           framePointer++;
                      }
                      else if(framePointer >= frames.length){
                           /*
                           Clock algorithm
                           while (page to evict not found aka the victim)
                              if(used/referenced bit for current page is false/0)
                                   replace current page
                              else
                                   reset used/referenced bit
                              advance clock pointer
                           */
                           //start clock
                           int victim = 0;
                           boolean victimFound = false;
                           while(!victimFound){//start while
                                pte_32 tempClock = pageTable.get(frames[clockPointer]);
                                if(!tempClock.referenced){
                                     victim = frames[clockPointer];
                                     victimFound = true;
                                }
                                else{
                                     pageTable.get(frames[clockPointer]).resetUsedBit();
                                }
                                clockPointer++;
                                //reset to zero if clockpointer goes too high
                                if(clockPointer == frames.length){
                                     clockPointer = 0;
                                }
                           }//end while
                           //end clock
                           //write to disk
                           pte_32 temp = pageTable.get(victim);
                           if(!temp.dirty){
                                System.out.println("page fault - evict clean: " + line);
                           }
                           else if(temp.dirty){
                                System.out.println("page fault - evict dirty: " + line);
                                diskWrites++;
                           }
                           frames[temp.physicalAddress] = pageEntry.presentLocation;
                           pageEntry.physicalAddress = temp.physicalAddress;
                           pageEntry.valid = true;
                           temp.setBooleanFalse();
                           temp.resetPhysicalAddress();
                           pageTable.put(victim, temp);
                      }

                      pageTable.put(pageNumber, pageEntry);
                      memoryAccesses++;
               } // end while loop
               inFile.close();

               System.out.println("Algorithm: Clock");
               System.out.println("Number of Frames: " + frames.length);
               System.out.println("Total memory accesses: " + memoryAccesses);
               System.out.println("Total page faults: " + pageFaults);
               System.out.println("Total writes to disk: " + diskWrites);
          }
          catch(FileNotFoundException e){

          }
     }
     public static void aging(int refresh, String tracefile){
          //Implement the aging algorithm that approximates LRU with an 8-bit counter
          /*
          The aging algorithm is a descendant of the NFU algorithm,
          with modifications to make it aware of the time span of use.
          Instead of just incrementing the counters of pages referenced,
          putting equal emphasis on page references regardless of the time,
          the reference counter on a page is first shifted right (divided by 2),
          before adding the referenced bit to the left of that binary number.
          For instance, if a page has referenced bits 1,0,0,1,1,0 in the past 6 clock ticks,
          its referenced counter will look like
          this: 10000000, 01000000, 00100000, 10010000, 11001000, 01100100.
          Page references closer to the present time have more impact than page references long ago.
          This ensures that pages referenced more recently,
          though less frequently referenced,
          will have higher priority over pages more frequently referenced in the past.
          Thus, when a page needs to be swapped out,
          the page with the lowest counter will be chosen.
          */
          int memoryAccesses = 0;
          int pageFaults = 0;
          int diskWrites = 0;
          try{
               Scanner inFile = new Scanner(new File(tracefile));

               int framePointer = 0;
               int clockPointer = 0;
               while (inFile.hasNext()) { //start while loop
                    //check refresh
                    if(refresh > 0 && memoryAccesses % refresh == 0){
                         System.out.println("Refreshed at: " + memoryAccesses);
                         for(int i = 0; i < frames.length; i++){
                              //reset R bit
                              pageTable.get(frames[i]).resetUsedBit();
                         }
                    }
                    for(int i = 0; i < frames.length; i++){
                         pte_32 temp = pageTable.get(frames[i]);
                         temp.bitCounter = temp.bitCounter >> 1;
                         if(temp.referenced){
                              temp.bitCounter = temp.bitCounter | 128;
                         }
                         if(temp.bitCounter > 128){
                              temp.bitCounter = temp.bitCounter & 0xFF000000;
                         }
                         pageTable.get(frames[i]).bitCounter = temp.bitCounter;
                    }
                    //begin reading file
                    String line = inFile.nextLine();
                    int pageNumber = Integer.decode("0x" + line.substring(0, 5));
                    char readWrite = line.toCharArray()[line.length()-1];
                    pte_32 pageEntry = pageTable.get(pageNumber);
                    pageEntry.presentLocation = pageNumber;
                    pageEntry.bitCounter = 0;
                    pageEntry.referenced = true;
                    if(readWrite == 'W'){
                         pageEntry.dirty = true;
                    }
                    if(pageEntry.valid){
                    //no page fault
                         pageTable.put(pageNumber, pageEntry);
                         memoryAccesses++;
                         System.out.println("hit: " + line);
                         continue;
                    }
                    //page fault
                    pageFaults++;
                    if(framePointer < frames.length){
                         frames[framePointer] = pageNumber;
                         pageEntry.physicalAddress = framePointer;
                         pageEntry.valid = true;
                         framePointer++;
                         System.out.println("page fault - no eviction: " + line);
                     }
                     //aging algorithm
                     else if(framePointer >= frames.length){

                           //write to disk
                           pte_32 victim = null;
                           boolean victimFound = false;
                           pte_32 temp = null;

                           int minIndex = -1;
                           int minAge = 0xFF;
                           while (!victimFound){
                                //linearly search all frames
                                for(int i = 0; i < frames.length; i++){
                                   temp = pageTable.get(frames[i]);
                                   if(temp.bitCounter < minAge){
                                        minIndex = i;
                                        victim = temp;
                                        victimFound = true;
                                        minAge = temp.bitCounter;
                                   }
                               }
                               if(!victim.dirty){
                                    System.out.println("page fault - evict clean: " + line);
                               }
                               if(victim.dirty){
                                    System.out.println("page fault - evict dirty: " + line);
                                    diskWrites++;
                               }
                               pageEntry.physicalAddress = victim.physicalAddress;
                               frames[pageEntry.physicalAddress] = pageEntry.presentLocation;
                               victim.setBooleanFalse();
                               victim.resetPhysicalAddress();
                               pageTable.put(victim.presentLocation, victim);
                               pageEntry.setValid(true);
                               pageTable.put(pageEntry.presentLocation, pageEntry);
                         }
                      }
                      pageTable.put(pageNumber, pageEntry);
                      memoryAccesses++;
               } // end while loop
               inFile.close();

               System.out.println("Algorithm: Aging");
               System.out.println("Number of Frames: " + frames.length);
               System.out.println("Refresh Rate: " + refresh);
               System.out.println("Total memory accesses: " + memoryAccesses);
               System.out.println("Total page faults: " + pageFaults);
               System.out.println("Total writes to disk: " + diskWrites);
          }
          catch(FileNotFoundException e){

          }
          catch(NullPointerException e){
               System.out.println("Algorithm: Aging");
               System.out.println("Number of Frames: " + frames.length);
               System.out.println("Refresh Rate: " + refresh);
               System.out.println("Total memory accesses: " + memoryAccesses);
               System.out.println("Total page faults: " + pageFaults);
               System.out.println("Total writes to disk: " + diskWrites);
          }
     }
     public static void workSetClock(int tau, int refresh, String tracefile){
          /*
          Use the line number of the file as your virtual time.

          Use the tau you specify on the command line to determine that a page is out of the working set.

          We will use a time periodic refresh as in NRU to reset the valid pages’ R bits back to zero.
          Use the command line parameter to set this time period. When resetting the R bit to zero,
          record the current virtual time in your page table entry for that page.

          We will use the following algorithm variant. On a page fault with no free frames available:

          Scan the page table looking at valid pages, each time continuing from where you left off previously
          If you find one that is unreferenced and clean, evict it and stop.
          Along the way, if you encounter a page that is unreferenced, older than tau, and dirty, write it out to disk and mark it as clean.
          If you get through the whole page table with no unreferenced, clean pages, evict the page with the oldest timestamp
          */
          int memoryAccesses = 0;
          int pageFaults = 0;
          int diskWrites = 0;
          try{
               Scanner inFile = new Scanner(new File(tracefile));
               memoryAccesses = 0;
               pageFaults = 0;
               diskWrites= 0;
               int framePointer = 0;
               int clockPointer = 0;
               while (inFile.hasNext()) { //start while loop
                    //check refresh
                    if(refresh > 0 && memoryAccesses % refresh == 0){
                         System.out.println("Refreshed at: " + memoryAccesses);
                         tau += memoryAccesses;
                         for(int i = 0; i < frames.length; i++){
                              //reset R bit
                              pageTable.get(frames[i]).resetUsedBit();
                         }
                    }
                    //begin reading file
                    String line = inFile.nextLine();
                    int pageNumber = Integer.decode("0x" + line.substring(0, 5));
                    char readWrite = line.toCharArray()[line.length()-1];
                    pte_32 pageEntry = pageTable.get(pageNumber);
                    pageEntry.lastUse = memoryAccesses;
                    pageEntry.presentLocation = pageNumber;
                    pageEntry.referenced = true;
                    if(readWrite == 'W'){
                         pageEntry.dirty = true;
                    }
                    if(pageEntry.valid){
                    //no page fault
                         pageEntry.lastUse = memoryAccesses;
                         pageTable.put(pageNumber, pageEntry);
                         memoryAccesses++;
                         System.out.println("hit: " + line);
                         continue;
                    }
                    //page fault
                    pageFaults++;
                    if(framePointer < frames.length){
                         frames[framePointer] = pageNumber;
                         pageEntry.physicalAddress = framePointer;
                         pageEntry.valid = true;
                         framePointer++;
                         System.out.println("page fault - no eviction: " + line);
                     }
                     //working set clock algorithm
                     else if(framePointer >= frames.length){
                          //scan page table looking at valid pages
                          //each time continuing from where you left off
                          //start clock
                          int victim = 0;
                          boolean victimFound = false;
                          int oldestIndex = -1;
                          int oldestAge = -1;
                          int startingClockPos = clockPointer;
                          //while(!victimFound){//start while
                          do{//start while
                               pte_32 tempClock = pageTable.get(frames[clockPointer]);
                               if(tempClock.lastUse > oldestAge){
                                    oldestAge = tempClock.lastUse;
                                    oldestIndex = clockPointer;
                               }
                               //if you find one that if unreferenced and clean
                               //evict it and stop
                               if(!tempClock.referenced){
                                    if(!tempClock.dirty){
                                         victim = frames[clockPointer];
                                         victimFound = true;
                                         break;
                                    }
                                    else{
                                         //not in working set
                                         if(tempClock.lastUse > tau){
                                              //schedule to evict
                                              pageTable.get(frames[clockPointer]).dirty = false;
                                         }
                                         //else in working set
                                         //pageTable.get(frames[clockPointer]).resetUsedBit();
                                    }

                                    //advance clock pointer
                                    clockPointer++;
                                    //reset to zero if clockpointer goes too high
                                    if(clockPointer == frames.length){
                                         clockPointer = 0;
                                    }
                                    //if you finish the entire clock, evict the oldest;
                                   if(startingClockPos == clockPointer){
                                        victim = frames[oldestIndex];
                                        victimFound = true;
                                        break;
                                   }
                              }
                              if(!victimFound && tempClock.referenced && startingClockPos == clockPointer){
                                   victim = frames[oldestIndex];
                                   victimFound = true;
                              }
                          //}//end while
                     }while(startingClockPos != clockPointer);
                          //end clock
                          //write to disk
                          pte_32 temp = pageTable.get(victim);
                          if(!temp.dirty){
                               System.out.println("page fault - evict clean: " + line);
                          }
                          else if(temp.dirty){
                               System.out.println("page fault - evict dirty: " + line);
                               diskWrites++;
                          }
                          frames[temp.physicalAddress] = pageEntry.presentLocation;
                          pageEntry.physicalAddress = temp.physicalAddress;
                          pageEntry.valid = true;
                          temp.setBooleanFalse();
                          temp.resetPhysicalAddress();
                          pageTable.put(victim, temp);
                      }
                      pageEntry.lastUse = memoryAccesses;
                      pageTable.put(pageNumber, pageEntry);
                      memoryAccesses++;
               } // end while loop
               inFile.close();

               System.out.println("Algorithm: Work Set Clock");
               System.out.println("Number of Frames: " + frames.length);
               System.out.println("Refresh Rate: " + refresh);
               System.out.println("Tau: " + tau);
               System.out.println("Total memory accesses: " + memoryAccesses);
               System.out.println("Total page faults: " + pageFaults);
               System.out.println("Total writes to disk: " + diskWrites);
          }
          catch(FileNotFoundException e){

          }
          catch(NullPointerException e){
               System.out.println("Algorithm: Aging");
               System.out.println("Number of Frames: " + frames.length);
               System.out.println("Refresh Rate: " + refresh);
               System.out.println("Total memory accesses: " + memoryAccesses);
               System.out.println("Total page faults: " + pageFaults);
               System.out.println("Total writes to disk: " + diskWrites);
          }
     }
}
class pte_32{
     public int presentLocation; //default 0, virtual address
     public boolean dirty; //default false
     public boolean referenced; //default false
     public boolean valid; //default false
     public int physicalAddress; //default -1, index in frame array
     public int lastUse;
     public int bitCounter;
     public pte_32(int presentLocation, boolean dirty, boolean referenced, boolean valid, int physicalAddress){
          this.presentLocation = presentLocation;
          this.dirty = dirty;
          this.referenced = referenced;
          this.valid = valid;
          this.physicalAddress = physicalAddress;
          //bitCounter = new BitSet(8);
     }
     public void setPresentLocation(int presentLocation){
          this.presentLocation = presentLocation;
     }
     public void setDirty(boolean dirty){
          this.dirty = dirty;
     }
     public void setReferenced(boolean referenced){
          this.referenced = referenced;
     }
     public void setValid(boolean valid){
          this.valid = valid;
     }
     public void setPhysicalAddress(int physicalAddress){
          this.physicalAddress = physicalAddress;
     }
     public void setBooleanFalse(){
          dirty = false;
          referenced = false;
          valid = false;
     }
     public void resetUsedBit(){
          referenced = false;
     }
     public void resetPhysicalAddress(){
          physicalAddress = -1;
     }

}
