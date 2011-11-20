// $Id: Ululator.java,v 1.1 2007/08/28 01:20:40 jamesll Exp jamesll $

/* $Log: Ululator.java,v $
 * Revision 1.1  2007/08/28 01:20:40  jamesll
 * Initial revision
 *
 */

package Sim;

import java.util.HashMap;
import java.util.Vector;
import java.util.Scanner;
import java.io.InputStream;
import java.util.Arrays;

public class Ululator{
    static int N, maxnode;
    static int nodes[];
    static int depth = 1;

    static public void main(String args[]){
				boolean bestRecover = false;

				// all layouts(nodes) at the current depth
				HashMap<Name, Node> curr = new HashMap<Name, Node>(maxnode);
				HashMap<Name, Node> next = null, last = null;



				if(args.length == 0){

						try{
								int max = 0;

								if(System.in.available() != 0){
										Scanner in = new Scanner(System.in);

										while(in.hasNext()){
												int itemp = in.nextInt();
												in.nextInt();

												if(itemp > max){
														max = itemp;
												}

												Name temp = new Name(itemp, 1);
												Node temp2 = new Node(temp, depth);

												curr.put(temp, temp2);

										}
								}else{
										System.out.println("Usage: java Ululator <width>");
										System.exit(2);
								}

								N = 1;
								maxnode = 1;

								while(maxnode < max){
										N++;
										maxnode = (1 << N) - 1;
								}
						}catch(Exception e){
								System.out.println("Usage: java Ululator <width> " + e);
								System.exit(2);
						}
				}else{
						N = Integer.parseInt(args[0]);
						maxnode = (1 << N) - 1;

						bestRecover = true;

						// make 1-disk layouts
						for(int i = 1; i <= maxnode; i++){
								Name temp = new Name(i, 1);
								Node temp2 = new Node(temp, depth);

								curr.put(temp, temp2);
						}
				}

				nodes = new int[curr.size()];
				int j = 0;

				for(Name nom: curr.keySet()){
						nodes[j] = nom.name[0];
						//System.out.println(nodes[j]);
						j++;
				}

				Arrays.sort(nodes);

				System.out.println("digraph before {");

				HashMap<Score,MarkovNode> currMarkov = null;
				HashMap<Score,MarkovNode> lastMarkov = null;

				MarkovNode zero = new MarkovNode(false);
				zero.numCoalesced++;
				zero.printMe(false);

				// grind on stuff for a while
				while(depth < nodes.length){
						// next level
						next = new HashMap<Name, Node>
								((int)choose(nodes.length,depth));

						//System.out.println("---- Depth ---- " + depth);

						currMarkov = new HashMap<Score,MarkovNode>();

						for(Node node: curr.values()){
								// check for life
								if(node.hadChildrenThatLived || node.name.checkIfLive()){
										//System.out.print(" Alive");
										node.alive = true;
								}else{
										//System.out.print(" Ded");
								}

								// set score
								if(!node.alive){
										node.relative.score[depth]++;
										node.comparative.score[depth]++;
								}

								MarkovNode m = null;

								if(node.alive){
										if(currMarkov.containsKey(node.comparative)){
												m = currMarkov.get(node.comparative);
										}else{
												m = new MarkovNode();
												currMarkov.put(node.comparative, m);
										}
										m.numCoalesced++;
								}

								//System.out.println("-- Node -- ");
								//node.name.print();
								//node.relative.print();

								// make parents11
								int i = 0, level = 0;

								while(level < node.name.name.length){

										for(; nodes[i] < node.name.name[level]; i++){

												// create name
												Name name = new Name(depth + 1);
												int k;
												for(k = 0; k < level; k++){
														name.name[k] = node.name.name[k];
												}

												name.name[k] = nodes[i];

												for(; k < node.name.name.length; k++){
														name.name[k+1] = node.name.name[k];
												}

												//System.out.println("- Parent - ");
												//name.print();

												Node parent;

												// find name
												if(!next.containsKey(name)){
														// create node
														parent = new Node(name, depth + 1);
														next.put(name, parent);
												}else{
														parent = next.get(name);
												}

												// add our scores to node
												if(node.mark == false){
														parent.relative.add(node.relative);
														node.mark = true;
												}

												if(node.alive){
														parent.hadChildrenThatLived = true;
												}

												parent.comparative.add(node.comparative);

												if(node.alive){
														parent.kids.add(m);
														m.incrementParent(parent);
												}else{
														parent.dedKids++;
												}
										}

										i++;
										level++;
								}

								/* This is a complete duplicate of the above code but
									 I can't figure out how to polish of these last few
									 using the above loop */
								for(; i < nodes.length; i++){
										// create name
										Name name = new Name(depth + 1);
										int k;
										for(k = 0; k < node.name.name.length; k++){
												name.name[k] = node.name.name[k];
										}

										name.name[k] = nodes[i];

										//System.out.println("- Parent - ");
										//name.print();

										Node parent;

										// find name
										if(!next.containsKey(name)){
												// create node
												parent = new Node(name, depth + 1);
												next.put(name, parent);
										}else{
												parent = next.get(name);
										}

										// add our scores to node
										if(node.mark == false){
												parent.relative.add(node.relative);
												node.mark = true;
										}

										if(node.alive){
												parent.hadChildrenThatLived = true;
										}

										parent.comparative.add(node.comparative);

										if(node.alive){
												parent.kids.add(m);
												m.incrementParent(parent);
										}else{
												parent.dedKids++;
										}
								}

								if(m != null){
										for(MarkovNode kid: node.kids){
												m.increment(kid);
										}

										m.dedKids += node.dedKids;
								}
						}

						MarkovNode maxN = null;
						Score maxScore = null;

						for(Score val: currMarkov.keySet()){
								if(maxScore == null || val.compare(maxScore) > 0){
										maxScore = val;
								}
						}

						maxN = currMarkov.get(maxScore);

						for(MarkovNode node: currMarkov.values()){
								if(node == maxN){
										node.printMe(true);
								}else{
										node.printMe(false);
								}
						}

						System.out.flush();


						depth++;

						if(lastMarkov != null){
								for(MarkovNode old: lastMarkov.values()){
										if(bestRecover){
												old.processParentsBestRecover(currMarkov);
										}else{
												old.processParents(currMarkov);
										}
								}
						}
						lastMarkov = currMarkov;

						//last = curr;
						curr = next;
				}

				// print out last remaining node

				if(curr.size() != 1){
						System.out.println("Curr size: " + curr.size());
				}

				for(Node node: curr.values()){
						if(node.hadChildrenThatLived || node.name.checkIfLive()){
								node.alive = true;
						}

						// set score
						if(!node.alive){
								node.relative.score[depth]++;
								node.comparative.score[depth]++;
						}
						//node.relative.count[depth]++;
						//node.comparative.count[depth]++;

						MarkovNode m;

						m = new MarkovNode();

						m.numCoalesced++;

						for(MarkovNode kid: node.kids){
								m.increment(kid);
						}

						m.dedKids += node.dedKids;

						currMarkov = new HashMap<Score,MarkovNode>();

						currMarkov.put(node.comparative, m);

						for(MarkovNode old: lastMarkov.values()){
								if(bestRecover){
										old.processParentsBestRecover(currMarkov);
								}else{
										old.processParents(currMarkov);
								}
						}

						m.printMe(true);

						//node.relative.print();
						//System.out.println();
						//node.comparative.print();
				}

				System.out.println("}");
    }

    public static long choose( long n, long m){
				long top = 1;

				for(long i = n; i > m; i--){
						top *= i;
				}

				for(long i = n-m; i > 1; i--){
						top /= i;
				}

				return top;
    }

}

class Node{
    Name name;

    Score relative;
    Score comparative;

    boolean hadChildrenThatLived, alive, mark;

    int dedKids;
    Vector<MarkovNode> kids;

    Node(Name name, int depth){
				this.name = name;
				dedKids = 0;
				kids = new Vector<MarkovNode>();

				relative = new Score(depth);
				comparative = new Score(depth);

				mark = false;
				hadChildrenThatLived = false;
    }
}

class Name{
    int name[];

    /* XXX: add hashCode() and equals() */

    public boolean equals(Object o){
				Name other = (Name)o;

				if(name.length != other.name.length){
						assert(false);
						return false;
				}

				for(int i = 0; i < name.length; i++){
						if(name[i] != other.name[i]){
								return false;
						}
				}

				return true;
    }

    public int hashCode(){
				int hash = 0;

				for(int temp: name){
						hash = (temp ^ (hash << 1));
				}

				return hash;
    }

    Name(int size){
				name = new int[size];
    }

    Name(int node, int size){
				name = new int[size];
				name[0] = node;
    }

    public boolean checkIfLive(){
				boolean flag = false;

				// iterate over all Data disks
				for(int i = 1; i < Ululator.maxnode; i <<= 1){
						flag = false;

						// check name array has data node, if so, its not dead, so don't
						// try to recover it
						for(int temp: name){
								if(temp == i){
										flag = true;
										break;
								}
						}

						// if disk is dead, try to get it back
						if(!flag){
								if(recurseCheck(name.length-1, i) == false){
										//System.out.print(" -DEAD - "); print();
										return false;
								}else{
										flag = true;
								}
						}
				}

				return flag;
    }

    boolean recurseCheck(int depth, int bitmap){

				for(; depth > 0; depth--){
						if( (bitmap ^ name[depth]) == 0 ||
								recurseCheck( (depth - 1), (bitmap ^ name[depth])) ){
								return true;
						}
				}

				/* maybe this is premature optimization, but I figure this
					 isn't worth another function call.  Also avoids a preamble
					 check on EVERY function call */
				if((bitmap ^ name[0]) == 0){
						return true;
				}else{
						return false;
				}
    }

    void print(){
				for(int i: name){
						System.out.print(" " + i);
				}
				System.out.println();
    }
}

class Score{
    int score[];

    public boolean equals(Object o){
        Score other = (Score)o;

        if(score.length != other.score.length){
            assert(false);
            return false;
        }

        for(int i = 0; i < score.length; i++){
            if(score[i] != other.score[i]){
                return false;
            }
        }

        return true;
    }

    public int compare(Object o){
        Score other = (Score)o;

        if(score.length != other.score.length){
            assert(false);
            return other.score.length - score.length;
        }

        for(int i = 0; i < score.length; i++){
            if(score[i] != other.score[i]){
                return other.score[i] - score[i];
            }
        }

        return 0;
    }


    public int hashCode(){
        int hash = 0;

        for(int temp: score){
            hash = (temp ^ (hash << 1));
        }

        return hash;
    }

    void add(Score other){
				for(int i = 0; i < other.score.length; i++){
						score[i] += other.score[i];
				}
    }

    Score(int depth){
				score = new int[depth+1];

				for(int i = 0; i <= depth; i++){
						score[i] = 0;
				}
    }

    public void print(){
				int i = score.length - 1;
				for(; i >= 0; i--){
						int combinations = (int)Ululator.choose(Ululator.nodes.length,i);

						System.out.println((Ululator.nodes.length - i) + " " +
															 (combinations - score[i]) + " " +
															 combinations);
				}
    }
}

class MarkovNode{
    HashMap<MarkovNode, Integer> kids;
    HashMap<Node, Integer> parents;
    int dotname, dedKids, numCoalesced;

    static int count = 0;

    MarkovNode(){
				kids = new HashMap<MarkovNode, Integer>();

				parents = new HashMap<Node, Integer>();

				dedKids = 0;

				count++;
				dotname = count; //+ Ululator.depth*1000;
    }

    // make the death node
    MarkovNode(boolean foo){
				dotname = 0;
				dedKids = 0;

				numCoalesced = 0;
    }

    void processParents(java.util.HashMap<Score,MarkovNode> lookup){
				//MarkovNode max = null;
				HashMap<MarkovNode,Integer> remap = new HashMap<MarkovNode,Integer>();

				for(Node parent: parents.keySet()){
						MarkovNode p = lookup.get(parent.comparative);

						int q = 0;
						if(remap.containsKey(p)){
								q = remap.get(p);
						}

						q += parents.get(parent);

						remap.put(p, q);
				}

				for(MarkovNode p: remap.keySet()){
						float decimal = (float)remap.get(p) / (float)numCoalesced;
						int natural = remap.get(p) / numCoalesced;

						if(((float)natural) == decimal){
								System.out.println("n"+dotname+" -> n"+p.dotname+
																	 " [label=<"+ natural +"&#956;>]");
						}else{
								System.out.println("n"+dotname+" -> n"+p.dotname+
																	 " [label=<"+ decimal +"&#956;>]");
						}
				}
    }

    void processParentsBestRecover(java.util.HashMap<Score,MarkovNode> lookup){
				MarkovNode max = null;
				Score maxScore = null;
				int count = 0;

				for(Node parent: parents.keySet()){
						MarkovNode p = lookup.get(parent.comparative);

						count += parents.get(parent);

						if(max == null){
								max = p;
								maxScore = parent.comparative;
						}else{
								if(parent.comparative.compare(maxScore) > 0){
										max = p;
										maxScore = parent.comparative;
								}
						}
				}

				float decimal = (float)count / (float)numCoalesced;
				int natural = count / numCoalesced;

				if(((float)natural) == decimal){
						System.out.println("n"+dotname+" -> n"+max.dotname+
															 " [style=\"bold\" constraint=false]");//label=<"+ natural +" &#956;>
				}else{
						System.out.println("n"+dotname+" -> n"+max.dotname+
															 " [style=\"bold\" constraint=false]"); //label=<"+ decimal +"&#956;>
				}
    }


    void incrementParent(Node parent){
				int temp = 0;

				if(parents.containsKey(parent)){
						temp = parents.get(parent);
				}

				temp++;

				parents.put(parent, temp);
    }

    void increment(MarkovNode kid){
				int temp = 0;

				if(kids.containsKey(kid)){
						temp = kids.get(kid);
				}

				temp++;

				kids.put(kid, temp);
    }

    void printMe(boolean max){
				if(dotname == 0){
						System.out.println("n"+dotname+" [label=\"Death\"]");
				}else{
						if(max){
								System.out.println("n"+dotname+" [label=\""+Ululator.depth+
																	 " "+numCoalesced+"\" style=\"bold\"]");
						}else{
								System.out.println("n"+dotname+" [label=\""+Ululator.depth+
																	 " "+numCoalesced+"\"]");
						}
				}

				if(kids != null){
						for(MarkovNode node: kids.keySet()){
								float decimal = (float)kids.get(node) / (float)numCoalesced;
								int natural = kids.get(node) / numCoalesced;

								if(((float)natural) == decimal){
										System.out.println("n"+dotname+" -> n"+node.dotname
																			 );//" [label=<"+ natural +" &#955;>]");
								}else{
										System.out.println("n"+dotname+" -> n"+node.dotname
																			 );//" [label=<"+ decimal +" &#955;>]");
								}
						}
				}

				if(dedKids != 0){
						float decimal = (float)dedKids / (float)numCoalesced;
						int natural = dedKids / numCoalesced;

						if(((float)natural) == decimal){
								System.out.println("n"+dotname+" -> n0 [label=<"+
																	 natural +" &#955;>]");
						}else{
								System.out.println("n"+dotname+" -> n0 [label=<"+
																	 decimal +" &#955;>]");
						}
				}
    }
}
