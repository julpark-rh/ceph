// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */



#ifndef __CDIR_H
#define __CDIR_H

#include "include/types.h"
#include "include/buffer.h"
#include "mdstypes.h"
#include "config.h"
#include "common/DecayCounter.h"

#include <iostream>
#include <cassert>

#include <list>
#include <set>
#include <map>
#include <string>
using namespace std;

#include <ext/hash_map>
using __gnu_cxx::hash_map;


#include "CInode.h"

class CDentry;
class MDCache;
class MDCluster;
class Context;


// -- authority delegation --
// directory authority types
//  >= 0 is the auth mds
#define CDIR_AUTH_PARENT   -1   // default
#define CDIR_AUTH_UNKNOWN  -2
#define CDIR_AUTH_DEFAULT   pair<int,int>(-1, -2)
#define CDIR_AUTH_UNDEF     pair<int,int>(-2, -2)
#define CDIR_AUTH_ROOTINODE pair<int,int>( 0, -2)


ostream& operator<<(ostream& out, class CDir& dir);


// CDir
typedef map<string, CDentry*> CDir_map_t;


//extern int cdir_pins[CDIR_NUM_PINS];


class CDir : public MDSCacheObject {
 public:
  // -- pins --
  static const int PIN_CHILD =    0;
  static const int PIN_OPENED =   1;  // open by another node
  static const int PIN_WAITER =   2;  // waiter(s)
  //static const int PIN_IMPORT =   3;
  static const int PIN_EXPORT =   4;
  //static const int PIN_FREEZE =   5;
  //  static const int PIN_FREEZELEAF = 6;
  static const int PIN_PROXY =    7;  // auth just changed.
  static const int PIN_AUTHPIN =  8;
  static const int PIN_IMPORTING = 9;
  static const int PIN_EXPORTING = 10;
  static const int PIN_IMPORTBOUND = 11;
  static const int PIN_EXPORTBOUND = 12;
  static const int PIN_DIRTY =    15;
  static const int PIN_REQUEST =  16;
  static const int PIN_LOGGINGEXPORTFINISH = 17;
  const char *pin_name(int p) {
    switch (p) {
    case PIN_CHILD: return "child";
    case PIN_OPENED: return "opened";
    case PIN_WAITER: return "waiter";
      //case PIN_IMPORT: return "import";
    case PIN_EXPORT: return "export";
    case PIN_EXPORTING: return "exporting";
    case PIN_IMPORTING: return "importing";
    case PIN_IMPORTBOUND: return "importbound";
    case PIN_EXPORTBOUND: return "exportbound";
      //case PIN_FREEZE: return "freeze";
      //    case PIN_FREEZELEAF: return "freezeleaf";
    case PIN_PROXY: return "proxy";
    case PIN_AUTHPIN: return "authpin";
    case PIN_DIRTY: return "dirty";
    case PIN_REQUEST: return "request";
    case PIN_LOGGINGEXPORTFINISH: return "loggingexportfinish";
    default: assert(0);
    }
  }

  // -- state --
  static const unsigned STATE_AUTH =          (1<< 0);   // auth for this dir (hashing doesn't count)
  static const unsigned STATE_PROXY =         (1<< 1);   // proxy auth
  static const unsigned STATE_COMPLETE =      (1<< 2);   // the complete contents are in cache
  static const unsigned STATE_DIRTY =         (1<< 3);   // has been modified since last commit
  static const unsigned STATE_FROZENTREE =    (1<< 4);   // root of tree (bounded by exports)
  static const unsigned STATE_FREEZINGTREE =  (1<< 5);   // in process of freezing 
  static const unsigned STATE_FROZENDIR =     (1<< 6);
  static const unsigned STATE_FREEZINGDIR =   (1<< 7);
  static const unsigned STATE_COMMITTING =    (1<< 8);   // mid-commit
  static const unsigned STATE_FETCHING =      (1<< 9);   // currenting fetching
  static const unsigned STATE_DELETED =       (1<<10);
  //static const unsigned STATE_IMPORT =        (1<<11);   // flag set if this is an import.
  static const unsigned STATE_EXPORT    =     (1<<12);
  static const unsigned STATE_IMPORTBOUND =   (1<<13);
  static const unsigned STATE_EXPORTBOUND =   (1<<14);
  static const unsigned STATE_EXPORTING =     (1<<15);
  static const unsigned STATE_IMPORTING =     (1<<16);

  // common states
  static const unsigned STATE_CLEAN =  0;
  static const unsigned STATE_INITIAL = 0;

  // these state bits are preserved by an import/export
  // ...except if the directory is hashed, in which case none of them are!
  static const unsigned MASK_STATE_EXPORTED = 
  STATE_COMPLETE|STATE_DIRTY;
  static const unsigned MASK_STATE_IMPORT_KEPT = 
  //STATE_IMPORT|
  STATE_EXPORT
  |STATE_IMPORTING
  |STATE_IMPORTBOUND|STATE_EXPORTBOUND
  |STATE_FROZENTREE|STATE_PROXY;
  static const unsigned MASK_STATE_EXPORT_KEPT = 
  STATE_EXPORTING
  |STATE_IMPORTBOUND|STATE_EXPORTBOUND
  |STATE_FROZENTREE
  |STATE_FROZENDIR
  |STATE_EXPORT
  |STATE_PROXY;

  // -- rep spec --
  static const int REP_NONE =     0;
  static const int REP_ALL =      1;
  static const int REP_LIST =     2;


  static const int NONCE_EXPORT  = 1;


  // -- wait masks --
  static const int WAIT_DENTRY        = (1<<0);  // wait for item to be in cache
  static const int WAIT_COMPLETE      = (1<<1);  // wait for complete dir contents
  static const int WAIT_FREEZEABLE    = (1<<2);  // hard_pins removed
  static const int WAIT_UNFREEZE      = (1<<3);  // unfreeze
  static const int WAIT_AUTHPINNABLE = WAIT_UNFREEZE;
  static const int WAIT_IMPORTED     = (1<<4);  // import finish
  static const int WAIT_SINGLEAUTH   = (1<<5); 

  static const int WAIT_DNREAD       = (1<<20);
  static const int WAIT_DNLOCK       = (1<<21);
  static const int WAIT_DNUNPINNED   = (1<<22);
  static const int WAIT_DNPINNABLE   = (WAIT_DNREAD|WAIT_DNUNPINNED);
  static const int WAIT_DNREQXLOCK   = (1<<23);
  
  static const int WAIT_ANY  = (0xffffffff);
  static const int WAIT_ATFREEZEROOT = (WAIT_AUTHPINNABLE|WAIT_UNFREEZE);
  static const int WAIT_ATSUBTREEROOT = (WAIT_SINGLEAUTH);




 public:
  // context
  MDCache  *cache;

  CInode          *inode;  // my inode
  frag_t           frag;   // my frag

 protected:
  // contents
  CDir_map_t       items;              // non-null AND null
  size_t           nitems;             // # non-null
  size_t           nnull;              // # null

  // state
  version_t       version;
  version_t       committing_version;
  version_t       committed_version;
  version_t       projected_version; 

  // lock nesting, freeze
  int        auth_pins;
  int        nested_auth_pins;
  int        request_pins;


  // waiters
  multimap<int, Context*> waiting;  // tag -> context
  hash_map< string, multimap<int, Context*> >
                          waiting_on_dentry;

  // cache control  (defined for authority; hints for replicas)
  int              dir_rep;
  set<int>         dir_rep_by;      // if dir_rep == REP_LIST

  // popularity
  meta_load_t popularity[MDS_NPOP];

  // friends
  friend class Migrator;
  friend class CInode;
  friend class MDCache;
  friend class MDiscover;
  friend class MDBalancer;

  friend class CDirDiscover;
  friend class CDirExport;

 public:
  CDir(CInode *in, frag_t fg, MDCache *mdcache, bool auth);



  // -- accessors --
  inodeno_t ino()     { return inode->ino(); }          // deprecate me?
  dirfrag_t dirfrag() { return dirfrag_t(inode->ino(), frag); }

  CInode *get_inode()    { return inode; }
  CDir *get_parent_dir() { return inode->get_parent_dir(); }

  CDir_map_t::iterator begin() { return items.begin(); }
  CDir_map_t::iterator end() { return items.end(); }
  size_t get_size() { 
    return nitems; 
  }
  size_t get_nitems() { return nitems; }
  size_t get_nnull() { return nnull; }

  /*
  float get_popularity() {
    return popularity[0].get();
  }
  */
  

  // -- dentries and inodes --
 public:
  CDentry* lookup(const string& n) {
    map<string,CDentry*>::iterator iter = items.find(n);
    if (iter == items.end()) 
      return 0;
    else
      return iter->second;
  }

  CDentry* add_dentry( const string& dname, CInode *in=0, bool auth=true );
  CDentry* add_dentry( const string& dname, inodeno_t ino, bool auth=true );
  void remove_dentry( CDentry *dn );         // delete dentry
  void link_inode( CDentry *dn, inodeno_t ino );
  void link_inode( CDentry *dn, CInode *in );
  void unlink_inode( CDentry *dn );
 private:
  void link_inode_work( CDentry *dn, CInode *in );
  void unlink_inode_work( CDentry *dn );

  void remove_null_dentries();

  // -- authority --
  /*
   *     normal: <parent,unknown>   !subtree_root
   * delegation: <mds,unknown>       subtree_root
   *  ambiguous: <mds1,mds2>         subtree_root
   *             <parent,mds2>       subtree_root     
   */
  pair<int,int> dir_auth;

 public:
  pair<int,int> authority();
  pair<int,int> dentry_authority(const string& d);
  pair<int,int> get_dir_auth() { return dir_auth; }
  void set_dir_auth(pair<int,int> a, bool iamauth=false);
  void set_dir_auth(int a) { 
    set_dir_auth(pair<int,int>(a, CDIR_AUTH_UNKNOWN), false); 
  }
  bool auth_is_ambiguous() {
    return dir_auth.second != CDIR_AUTH_UNKNOWN;
  }
  bool is_fullauth() {
    return is_auth() && !auth_is_ambiguous();
  }
  bool is_fullnonauth() {
    return !is_auth() && !auth_is_ambiguous();
  }
  
  bool is_subtree_root();

 

  // for giving to clients
  void get_dist_spec(set<int>& ls, int auth) {
    if (( popularity[MDS_POP_CURDOM].pop[META_POP_IRD].get() > 
	  g_conf.mds_bal_replicate_threshold)) {
      //if (!cached_by.empty() && inode.ino > 1) dout(1) << "distributed spec for " << *this << endl;
      for (map<int,int>::iterator p = replicas_begin();
	   p != replicas_end(); 
	   ++p)
	ls.insert(p->first);
      if (!ls.empty()) 
	ls.insert(auth);
    }
  }


  // -- state --
  bool is_complete() { return state & STATE_COMPLETE; }
  bool is_dirty() { return state_test(STATE_DIRTY); }
  bool is_clean() { return !state_test(STATE_DIRTY); }

  bool is_auth() { return state & STATE_AUTH; }
  bool is_proxy() { return state & STATE_PROXY; }
  //bool is_import() { return state & STATE_IMPORT; }
  //bool is_export() { return state & STATE_EXPORT; }
  bool is_exporting() { return state & STATE_EXPORTING; }
  bool is_importing() { return state & STATE_IMPORTING; }

  bool is_hashed() { return false; }
  bool is_hashing() { return false; }

  bool is_rep() { 
    if (dir_rep == REP_NONE) return false;
    return true;
  }
 
  
  // -- fetch --
  object_t get_ondisk_object() { return object_t(ino(), frag); }
  void fetch(Context *c);
  void _fetched(bufferlist &bl);

  // -- commit --
  map<version_t, list<Context*> > waiting_for_commit;

  void commit_to(version_t want);
  void commit(version_t want, Context *c);
  void _commit(version_t want);
  void _committed(version_t v);
  void wait_for_commit(Context *c, version_t v=0);



  // -- dirtyness --
  version_t get_version() { return version; }
  void set_version(version_t v) { projected_version = version = v; }
  version_t get_projected_version() { return projected_version; }
  
  version_t get_committing_version() { return committing_version; }
  version_t get_committed_version() { return committed_version; }
  // as in, we're committing the current version.
  //void set_committing_version() { committing_version = version; }
  void set_committed_version(version_t v) { committed_version = v; }

  version_t pre_dirty(version_t min=0);
  void _mark_dirty();
  void mark_dirty(version_t pv);
  void mark_clean();
  void mark_complete() { state_set(STATE_COMPLETE); }




  // -- reference counting --
  void first_get();
  void last_put();

  void request_pin_get() {
    if (request_pins == 0) get(PIN_REQUEST);
    request_pins++;
  }
  void request_pin_put() {
    request_pins--;
    if (request_pins == 0) put(PIN_REQUEST);
  }

    
  // -- waiters --
  bool waiting_for(int tag);
  bool waiting_for(int tag, const string& dn);
  void add_waiter(int tag, Context *c);
  void add_waiter(int tag,
                  const string& dentry,
                  Context *c);
  void take_waiting(int mask, list<Context*>& ls);  // includes dentry waiters
  void take_waiting(int mask, 
                    const string& dentry, 
                    list<Context*>& ls,
                    int num=0);  
  void finish_waiting(int mask, int result = 0);    // ditto
  void finish_waiting(int mask, const string& dn, int result = 0);    // ditto


  // -- auth pins --
  bool can_auth_pin() { return is_auth() && !(is_frozen() || is_freezing()); }
  int is_auth_pinned() { return auth_pins; }
  int get_cum_auth_pins() { return auth_pins + nested_auth_pins; }
  int get_auth_pins() { return auth_pins; }
  int get_nested_auth_pins() { return nested_auth_pins; }
  void auth_pin();
  void auth_unpin();
  void adjust_nested_auth_pins(int inc);
  void on_freezeable();

  // -- freezing --
  void freeze_tree(Context *c);
  void freeze_tree_finish(Context *c);
  void unfreeze_tree();
  void _freeze_tree(Context *c=0);

  void freeze_dir(Context *c);
  void freeze_dir_finish(Context *c);
  void _freeze_dir(Context *c=0);
  void unfreeze_dir();

  bool is_freezing() { return is_freezing_tree() || is_freezing_dir(); }
  bool is_freezing_tree();
  bool is_freezing_tree_root() { return state & STATE_FREEZINGTREE; }
  bool is_freezing_dir() { return state & STATE_FREEZINGDIR; }

  bool is_frozen() { return is_frozen_dir() || is_frozen_tree(); }
  bool is_frozen_tree();
  bool is_frozen_tree_root() { return state & STATE_FROZENTREE; }
  bool is_frozen_dir() { return state & STATE_FROZENDIR; }
  
  bool is_freezeable() {
    // no nested auth pins.
    if (auth_pins > 0 || nested_auth_pins > 0) 
      return false;

    // inode must not be frozen.
    if (!is_subtree_root() && inode->is_frozen())
      return false;

    return true;
  }
  bool is_freezeable_dir() {
    if (auth_pins > 0) 
      return false;

    // if not subtree root, inode must not be frozen.
    if (!is_subtree_root() && inode->is_frozen())
      return false;

    return true;
  }

  CDir *get_frozen_tree_root();

};



// -- encoded state --

// discover

class CDirDiscover {
  dirfrag_t dirfrag;
  int       nonce;
  int       dir_rep;
  set<int>  rep_by;

 public:
  CDirDiscover() {}
  CDirDiscover(CDir *dir, int nonce) {
    dirfrag = dir->dirfrag();
    this->nonce = nonce;
    dir_rep = dir->dir_rep;
    rep_by = dir->dir_rep_by;
  }

  void update_dir(CDir *dir) {
    assert(dir->dirfrag() == dirfrag);
    assert(!dir->is_auth());

    dir->replica_nonce = nonce;
    dir->dir_rep = dir_rep;
    dir->dir_rep_by = rep_by;
  }

  dirfrag_t get_dirfrag() { return dirfrag; }

  
  void _encode(bufferlist& bl) {
    bl.append((char*)&dirfrag, sizeof(dirfrag));
    bl.append((char*)&nonce, sizeof(nonce));
    bl.append((char*)&dir_rep, sizeof(dir_rep));
    ::_encode(rep_by, bl);
  }

  void _decode(bufferlist& bl, int& off) {
    bl.copy(off, sizeof(dirfrag), (char*)&dirfrag);
    off += sizeof(dirfrag);
    bl.copy(off, sizeof(nonce), (char*)&nonce);
    off += sizeof(nonce);
    bl.copy(off, sizeof(dir_rep), (char*)&dir_rep);
    off += sizeof(dir_rep);
    ::_decode(rep_by, bl, off);
  }

};


// export

class CDirExport {
  struct {
    dirfrag_t   dirfrag;
    long        nden;   // num dentries (including null ones)
    version_t   version;
    version_t   committed_version;
    unsigned    state;
    meta_load_t popularity_justme;
    meta_load_t popularity_curdom;
    int         dir_rep;
  } st;
  map<int,int> replicas;
  set<int>     rep_by;

 public:
  CDirExport() {}
  CDirExport(CDir *dir) {
    memset(&st, 0, sizeof(st));

    assert(dir->get_version() == dir->get_projected_version());

    st.dirfrag = dir->dirfrag();
    st.nden = dir->items.size();
    st.version = dir->version;
    st.committed_version = dir->committed_version;
    st.state = dir->state;
    st.dir_rep = dir->dir_rep;

    st.popularity_justme.take( dir->popularity[MDS_POP_JUSTME] );
    st.popularity_curdom.take( dir->popularity[MDS_POP_CURDOM] );
    dir->popularity[MDS_POP_ANYDOM] -= st.popularity_curdom;
    dir->popularity[MDS_POP_NESTED] -= st.popularity_curdom;

    rep_by = dir->dir_rep_by;
    replicas = dir->replicas;
  }

  dirfrag_t get_dirfrag() { return st.dirfrag; }
  __uint64_t get_nden() { return st.nden; }

  void update_dir(CDir *dir) {
    assert(dir->dirfrag() == st.dirfrag);

    // set committed_version at old version
    dir->committing_version = dir->committed_version = st.committed_version;
    dir->projected_version = dir->version = st.version;

    // twiddle state
    dir->state = (dir->state & CDir::MASK_STATE_IMPORT_KEPT) |   // remember import flag, etc.
      (st.state & CDir::MASK_STATE_EXPORTED);
    dir->dir_rep = st.dir_rep;

    dir->popularity[MDS_POP_JUSTME] += st.popularity_justme;
    dir->popularity[MDS_POP_CURDOM] += st.popularity_curdom;
    dir->popularity[MDS_POP_ANYDOM] += st.popularity_curdom;
    dir->popularity[MDS_POP_NESTED] += st.popularity_curdom;

    dir->replica_nonce = 0;  // no longer defined

    if (!dir->replicas.empty())
      dout(0) << "replicas not empty non import, " << *dir << ", " << dir->replicas << endl;

    dir->dir_rep_by = rep_by;
    dir->replicas = replicas;
    dout(12) << "replicas in export is " << replicas << ", dir now " << dir->replicas << endl;
    if (!replicas.empty())
      dir->get(CDir::PIN_OPENED);
    if (dir->is_dirty()) {
      dir->get(CDir::PIN_DIRTY);  
    }
  }


  void _encode(bufferlist& bl) {
    bl.append((char*)&st, sizeof(st));
    ::_encode(replicas, bl);
    ::_encode(rep_by, bl);
  }

  int _decode(bufferlist& bl, int off = 0) {
    bl.copy(off, sizeof(st), (char*)&st);
    off += sizeof(st);
    ::_decode(replicas, bl, off);
    ::_decode(rep_by, bl, off);
    return off;
  }

};



#endif
