/* $Header: /data/zender/nco_20150216/nco/src/nco/nco_var_avg.c,v 1.33 2005-11-26 20:22:40 zender Exp $ */

/* Purpose: Average variables */

/* Copyright (C) 1995--2005 Charlie Zender
   This software may be modified and/or re-distributed under the terms of the GNU General Public License (GPL) Version 2
   See http://www.gnu.ai.mit.edu/copyleft/gpl.html for full license text */

#include "nco_var_avg.h" /* Average variables */

var_sct * /* O [sct] Partially (non-normalized) reduced variable */
nco_var_avg /* [fnc] Reduce given variable over specified dimensions */
(var_sct *var, /* I/O [sct] Variable to reduce (e.g., average) (destroyed) */
 dmn_sct * const * const dim, /* I [sct] Dimensions over which to reduce variable */
 const int nbr_dim, /* I [sct] Number of dimensions to reduce variable over */
 const int nco_op_typ) /* I [enm] Operation type, default is average */
{
  /* Threads: Routine is thread safe and calls no unsafe routines */
  /* Purpose: Reduce given variable over specified dimensions 
     "Reduce" means to reduce rank of variable by performing an arithmetic operation
     Output variable is duplicate of input variable, except for averaging dimensions
     Default operation is averaging, but nco_op_typ can also be min, max, etc.
     nco_var_avg() overwrites contents, if any, of tally array with number of valid reduction operations

     Input variable structure is destroyed and routine returns resized, partially reduced variable
     For some operations, such as min, max, ttl, variable returned by nco_var_avg() is complete and need not be further processed
     For averaging operation, output variable must be normalized by its tally array
     In other words, nco_var_nrm() should be called subsequently if normalization is desired
     Normalization is not done internally to nco_var_avg() to allow user more flexibility */ 

  /* Routine keeps track of three variables whose abbreviations are:
     var: Input variable (already hyperslabbed)
     avg: An array of averaging blocks, each a contiguous arrangement of all 
          elements of var which contribute to a single element of fix.
     fix: Output (averaged) variable */

  dmn_sct **dmn_avg;
  dmn_sct **dmn_fix;

  int idx_avg_var[NC_MAX_DIMS];
  /*  int idx_var_avg[NC_MAX_DIMS];*/ /* Variable is unused but instructive anyway */
  int idx_fix_var[NC_MAX_DIMS];
  /*  int idx_var_fix[NC_MAX_DIMS];*/ /* Variable is unused but instructive anyway */
  int idx;
  int idx_dmn;
  int dmn_avg_nbr;
  int nbr_dmn_fix;
  int nbr_dmn_var;

  long avg_sz;
  long fix_sz;
  long var_sz;

  var_sct *fix;

  /* Copy basic attributes of input variable into output (averaged) variable */
  fix=nco_var_dpl(var);

  /* Create lists of averaging and fixed dimensions (in order of their appearance 
     in the variable). We do not know a priori how many dimensions remain in the 
     output (averaged) variable, but nbr_dmn_var is an upper bound. Similarly, we do
     not know a priori how many of the dimensions in the input list of averaging 
     dimensions (dim) actually occur in the current variable, so we do not know
     dmn_avg_nbr, but nbr_dim is an upper bound on it. */
  nbr_dmn_var=var->nbr_dim;
  nbr_dmn_fix=0;
  dmn_avg_nbr=0;
  dmn_avg=(dmn_sct **)nco_malloc(nbr_dim*sizeof(dmn_sct *));
  dmn_fix=(dmn_sct **)nco_malloc(nbr_dmn_var*sizeof(dmn_sct *));
  for(idx=0;idx<nbr_dmn_var;idx++){
    for(idx_dmn=0;idx_dmn<nbr_dim;idx_dmn++){
      /* Comparing dimension IDs is faster than comparing dimension names 
	 but requires assumption that all dimensions are from same file */
      if(var->dmn_id[idx] == dim[idx_dmn]->id){
	/* Although structures in dim are never altered, linking them into
	   dmn_avg list makes them vulnerable to manipulation and forces 
	   dim to lose const protection in prototype */
	dmn_avg[dmn_avg_nbr]=dim[idx_dmn];
	/* idx_avg_var[i]=j means that the ith averaging dimension is the jth dimension of var */
	idx_avg_var[dmn_avg_nbr]=idx;
	/* idx_var_avg[j]=i means that the jth dimension of var is the ith averaging dimension */
	/*	idx_var_avg[idx]=dmn_avg_nbr;*/ /* Variable is unused but instructive anyway */
	dmn_avg_nbr++;
	break;
      } /* end if */
    } /* end loop over idx_dmn */
    if(idx_dmn == nbr_dim){
      dmn_fix[nbr_dmn_fix]=var->dim[idx];
      /* idx_fix_var[i]=j means that the ith fixed dimension is the jth dimension of var */
      idx_fix_var[nbr_dmn_fix]=idx;
      /* idx_var_fix[j]=i means that the jth dimension of var is the ith fixed dimension */
      /*      idx_var_fix[idx]=nbr_dmn_fix;*/ /* Variable is unused but instructive anyway */
      nbr_dmn_fix++;
    } /* end if */
  } /* end loop over idx */

  if(dmn_avg_nbr == 0){
    /* 20050517: ncwa only calls nco_var_avg() with variables containing averaging dimensions
       Variables without averaging dimensions are in the var_fix list 
       We preserve nco_var_avg() capability to work on var_fix variables for future flexibility */
    (void)fprintf(stderr,"%s: WARNING %s does not contain any averaging dimensions\n",prg_nm_get(),fix->nm);
    /* Variable does not contain any averaging dimensions so we are done
       For consistency, return copy of variable held in fix and free() original
       Hence, nco_var_avg() always destroys original input and returns valid output */
    goto cln_and_xit;
  } /* end if */

  /* Free extra list space */
  dmn_fix=(dmn_sct **)nco_realloc(dmn_fix,nbr_dmn_fix*sizeof(dmn_sct *));
  dmn_avg=(dmn_sct **)nco_realloc(dmn_avg,dmn_avg_nbr*sizeof(dmn_sct *));

  /* Get rid of averaged dimensions */
  fix->nbr_dim=nbr_dmn_fix;

  avg_sz=1L;
  for(idx=0;idx<dmn_avg_nbr;idx++){
    avg_sz*=dmn_avg[idx]->cnt;
    fix->sz/=dmn_avg[idx]->cnt;
    if(!dmn_avg[idx]->is_rec_dmn) fix->sz_rec/=dmn_avg[idx]->cnt;
  } /* end loop over idx */
  /* Convenience variable to avoid repetitive indirect addressing */
  fix_sz=fix->sz;

  fix->is_rec_var=False;
  for(idx=0;idx<nbr_dmn_fix;idx++){
    if(dmn_fix[idx]->is_rec_dmn) fix->is_rec_var=True;
    fix->dim[idx]=dmn_fix[idx];
    fix->dmn_id[idx]=dmn_fix[idx]->id;
    fix->srt[idx]=var->srt[idx_fix_var[idx]];
    fix->cnt[idx]=var->cnt[idx_fix_var[idx]];
    fix->end[idx]=var->end[idx_fix_var[idx]];
  } /* end loop over idx */
  
  fix->is_crd_var=False;
  if(nbr_dmn_fix == 1)
    if(dmn_fix[0]->is_crd_dmn) 
      fix->is_crd_var=True;

  /* Trim dimension arrays to their new sizes */
  fix->dim=(dmn_sct **)nco_realloc(fix->dim,nbr_dmn_fix*sizeof(dmn_sct *));
  fix->dmn_id=(int *)nco_realloc(fix->dmn_id,nbr_dmn_fix*sizeof(int));
  fix->srt=(long *)nco_realloc(fix->srt,nbr_dmn_fix*sizeof(long));
  fix->cnt=(long *)nco_realloc(fix->cnt,nbr_dmn_fix*sizeof(long));
  fix->end=(long *)nco_realloc(fix->end,nbr_dmn_fix*sizeof(long));
  
  /* If product of sizes of all averaging dimensions is 1, input and output value arrays should be identical 
     Since var->val was already copied to fix->val by nco_var_dpl() at the beginning
     of this routine, only one task remains, to set fix->tally appropriately. */
  if(avg_sz == 1L){
    long *fix_tally;

    fix_tally=fix->tally;

    /* First set tally field to 1 */
    for(idx=0;idx<fix_sz;idx++) fix_tally[idx]=1L;
    /* Next overwrite any missing value locations with zero */
    if(fix->has_mss_val){
      int val_sz_byte;

      char *val;
      char *mss_val;

      /* NB: Use char * rather than void * because some compilers (acc) will not do pointer
	 arithmetic on void * */
      mss_val=(char *)fix->mss_val.vp;
      val_sz_byte=nco_typ_lng(fix->type);
      val=(char *)fix->val.vp;
      for(idx=0;idx<fix_sz;idx++,val+=val_sz_byte)
	if(!memcmp(val,mss_val,(size_t)val_sz_byte)) fix_tally[idx]=0L;
    } /* fix->has_mss_val */
  } /* end if avg_sz == 1L */

  /* A complex and expensive collection procedure of generating averaging blocks
     works in all cases but is unnecessary in one important case.
     If the averaging dimensions are any number of the most rapidly varying (MRV) 
     dimensions, then no re-arrangement is necessary because the averaging blocks
     are the same as the original storage. 
     The averaging dimensions are the MRV dimensions iff the nbr_dmn_fix fixed 
     dimensions are one-to-one with the first nbr_dmn_fix input dimensions. 
     Another way to answer this question is to compare the dmn_avg_nbr averaging 
     dimensions with the last dmn_avg_nbr dimensions of the input variable.
     However, the averaging dimensions may appear in any order so it seems more
     straightforward to compare the fixed dimensions.  */
  if(avg_sz != 1L){
    for(idx=0;idx<nbr_dmn_fix;idx++) 
      if(idx_fix_var[idx] != idx) break;
    if(idx == nbr_dmn_fix){
      (void)fprintf(stderr,"%s: INFO Reduction dimensions are most-rapidly-varying (MRV) dimensions of %s. Possible to skip collection step and proceed straight to reduction step.\n",prg_nm_get(),fix->nm);
    } /* idx != nbr_dmn_fix */
  } /* end if avg_sz == 1L */

  /* Starting at first element of input hyperslab, add up next stride elements
     and place result in first element of output hyperslab. */
  if(avg_sz != 1L){
    char *avg_cp;
    char *var_cp;
    
    int typ_sz;
    int nbr_dmn_var_m1;

    long *var_cnt;
    long avg_lmn;
    long fix_lmn;
    long var_lmn;
    long dmn_ss[NC_MAX_DIMS];
    long dmn_var_map[NC_MAX_DIMS];
    long dmn_avg_map[NC_MAX_DIMS];
    long dmn_fix_map[NC_MAX_DIMS];

    ptr_unn avg_val;

    nbr_dmn_var_m1=nbr_dmn_var-1;
    typ_sz=nco_typ_lng(fix->type);
    var_cnt=var->cnt;
    var_cp=(char *)var->val.vp;
    var_sz=var->sz;
    
    /* Reuse existing value buffer (it is of size var_sz, created by nco_var_dpl()) */
    avg_val=fix->val;
    avg_cp=(char *)avg_val.vp;
    /* Create new value buffer for output (averaged) size */
    fix->val.vp=(void *)nco_malloc(fix_sz*nco_typ_lng(fix->type));
    /* Resize (or just plain allocate) tally array */
    fix->tally=(long *)nco_realloc(fix->tally,fix_sz*sizeof(long));

    /* Initialize value and tally arrays */
    (void)nco_zero_long(fix_sz,fix->tally);
    (void)nco_var_zero(fix->type,fix_sz,fix->val);
  
    /* Compute map for each dimension of input variable */
    for(idx=0;idx<nbr_dmn_var;idx++) dmn_var_map[idx]=1L;
    for(idx=0;idx<nbr_dmn_var-1;idx++)
      for(idx_dmn=idx+1;idx_dmn<nbr_dmn_var;idx_dmn++)
	dmn_var_map[idx]*=var->cnt[idx_dmn];
    
    /* Compute map for each dimension of output variable */
    for(idx=0;idx<nbr_dmn_fix;idx++) dmn_fix_map[idx]=1L;
    for(idx=0;idx<nbr_dmn_fix-1;idx++)
      for(idx_dmn=idx+1;idx_dmn<nbr_dmn_fix;idx_dmn++)
	dmn_fix_map[idx]*=fix->cnt[idx_dmn];
    
    /* Compute map for each dimension of averaging buffer */
    for(idx=0;idx<dmn_avg_nbr;idx++) dmn_avg_map[idx]=1L;
    for(idx=0;idx<dmn_avg_nbr-1;idx++)
      for(idx_dmn=idx+1;idx_dmn<dmn_avg_nbr;idx_dmn++)
	dmn_avg_map[idx]*=dmn_avg[idx_dmn]->cnt;
    
    /* var_lmn is offset into 1-D array */
    for(var_lmn=0;var_lmn<var_sz;var_lmn++){
      /* dmn_ss are corresponding indices (subscripts) into N-D array */
	/* Operations: 1 modulo, 1 pointer offset, 1 user memory fetch
	   Repetitions: \lmnnbr
	   Total Counts: \ntgnbr=2\lmnnbr, \mmrusrnbr=\lmnnbr
	   NB: LHS assumed compact and cached, counted RHS offsets and fetches only */
      dmn_ss[nbr_dmn_var_m1]=var_lmn%var_cnt[nbr_dmn_var_m1];
      for(idx=0;idx<nbr_dmn_var_m1;idx++){
	/* Operations: 1 divide, 1 modulo, 2 pointer offset, 2 user memory fetch
	   Repetitions: \lmnnbr(\dmnnbr-1)
	   Counts: \ntgnbr=4\lmnnbr(\dmnnbr-1), \mmrusrnbr=2\lmnnbr(\dmnnbr-1)
	   NB: LHS assumed compact and cached, counted RHS offsets and fetches only
	   NB: Neglected loop arithmetic/compare */
	dmn_ss[idx]=(long)(var_lmn/dmn_var_map[idx]);
	dmn_ss[idx]%=var_cnt[idx];
      } /* end loop over dimensions */

      /* Map variable's N-D array indices into a 1-D index into averaged data */
      fix_lmn=0L;
      /* Operations: 1 add, 1 multiply, 3 pointer offset, 3 user memory fetch
	 Repetitions: \lmnnbr(\dmnnbr-\avgnbr)
	 Counts: \ntgnbr=5\lmnnbr(\dmnnbr-\avgnbr), \mmrusrnbr=3\lmnnbr(\dmnnbr-\avgnbr) */
      for(idx=0;idx<nbr_dmn_fix;idx++) fix_lmn+=dmn_ss[idx_fix_var[idx]]*dmn_fix_map[idx];
      
      /* Map N-D array indices into 1-D offset from group offset */
      avg_lmn=0L;
      /* Operations: 1 add, 1 multiply, 3 pointer offset, 3 user memory fetch
	 Repetitions: \lmnnbr\avgnbr
	 Counts: \ntgnbr=5\lmnnbr\avgnbr, \mmrusrnbr=3\lmnnbr\avgnbr */
      for(idx=0;idx<dmn_avg_nbr;idx++) avg_lmn+=dmn_ss[idx_avg_var[idx]]*dmn_avg_map[idx];
      
      /* Copy current element in input array into its slot in sorted avg_val */
      /* Operations: 3 add, 3 multiply, 0 pointer offset, 1 system memory copy
	 Repetitions: \lmnnbr
	 Counts: \ntgnbr=6\lmnnbr, \mmrusrnbr=0, \mmrsysnbr=1 */
      (void)memcpy(avg_cp+(fix_lmn*avg_sz+avg_lmn)*typ_sz,var_cp+var_lmn*typ_sz,(size_t)typ_sz);
    } /* end loop over var_lmn */
    
    /* Input data are now sorted and stored (in avg_val) in blocks (of length avg_sz)
       in same order as blocks' average values will appear in output buffer. 
       Averaging routines can take advantage of this by casting avg_val to 
       two dimensional variable and averaging over inner dimension. 
       Tally array is actually set in nco_var_avg_reduce_*() */
    switch(nco_op_typ){
    case nco_op_max:
      (void)nco_var_avg_reduce_max(fix->type,var_sz,fix_sz,fix->has_mss_val,fix->mss_val,avg_val,fix->val);
      break;
    case nco_op_min:
      (void)nco_var_avg_reduce_min(fix->type,var_sz,fix_sz,fix->has_mss_val,fix->mss_val,avg_val,fix->val);
      break;
    case nco_op_avg: /* Operations: Previous=none, Current=sum, Next=normalize and root */
    case nco_op_sqravg: /* Operations: Previous=none, Current=sum, Next=normalize and square */
    case nco_op_avgsqr: /* Operations: Previous=square, Current=sum, Next=normalize */
    case nco_op_rms: /* Operations: Previous=square, Current=sum, Next=normalize and root */
    case nco_op_rmssdn: /* Operations: Previous=square, Current=sum, Next=normalize and root */
    case nco_op_ttl: /* Operations: Previous=none, Current=sum, Next=none */
    default:
      (void)nco_var_avg_reduce_ttl(fix->type,var_sz,fix_sz,fix->has_mss_val,fix->mss_val,fix->tally,avg_val,fix->val);	  		
      break;
    } /* end case */

    /* Free dynamic memory that held rearranged input variable values */
    avg_val.vp=nco_free(avg_val.vp);
  } /* end if avg_sz != 1 */
  
  /* Jump here if variable is not averaged */
 cln_and_xit:

  /* Free input variable */
  var=nco_var_free(var);
  dmn_avg=(dmn_sct **)nco_free(dmn_avg);
  dmn_fix=(dmn_sct **)nco_free(dmn_fix);

  /* Return averaged variable */
  return fix;
} /* end nco_var_avg() */

void
nco_var_avg_reduce_ttl /* [fnc] Sum blocks of op1 into each element of op2 */
(const nc_type type, /* I [enm] netCDF type of operands */
 const long sz_op1, /* I [nbr] Size (in elements) of op1 */
 const long sz_op2, /* I [nbr] Size (in elements) of op2 */
 const int has_mss_val, /* I [flg] Operand has missing values */
 ptr_unn mss_val, /* I [sct] Missing value */
 long * const tally, /* I/O [nbr] Tally buffer */
 ptr_unn op1, /* I [sct] Operand (sz_op2 contiguous blocks of size (sz_op1/sz_op2)) */
 ptr_unn op2) /* O [sct] Sum of each block of op1 */
{
  /* Threads: Routine is thread safe and calls no unsafe routines */
  /* Purpose: Sum values in each contiguous block of first operand and place
     result in corresponding element in second operand. 
     Currently arithmetic operation performed is summation of elements in op1
     Input operands are assumed to have conforming types, but not dimensions or sizes
     nco_var_avg_reduce() knows nothing about dimensions
     Routine is one dimensional array operator acting serially on each element of input buffer op1
     Calling rouine knows exactly how rank of output, op2, is reduced from rank of input
     Routine only does summation rather than averaging in order to remain flexible
     Operations which require normalization, e.g., averaging, must call nco_var_nrm() 
     or nco_var_dvd() to divide sum set in this routine by tally set in this routine. */

  /* Each operation has GNUC and non-GNUC blocks:
     GNUC: Utilize (non-ANSI-compliant) compiler support for local automatic arrays
     This results in more elegent loop structure and, theoretically, in faster performance
     20040225: In reality, the GNUC non-ANSI blocks fail on some large files
     This may be because they allocate significant local storage on the stack
     
     non-GNUC: Fully ANSI-compliant structure
     Fortran: Support deprecated */

#define FXM_NCO315 1
#ifdef FXM_NCO315
  long idx_op1;
#endif /* __GNUC__ */
  const long sz_blk=sz_op1/sz_op2;
  long idx_op2;
  long idx_blk;
  double mss_val_dbl=double_CEWI;
  float mss_val_flt=float_CEWI;
  nco_char mss_val_chr;
  nco_byte mss_val_byt;
  nco_int mss_val_lng=nco_int_CEWI;
  short mss_val_sht=short_CEWI;

  /* Typecast pointer to values before access */
  (void)cast_void_nctype(type,&op1);
  (void)cast_void_nctype(type,&op2);
  if(has_mss_val) (void)cast_void_nctype(type,&mss_val);

  if(has_mss_val){
    switch(type){
    case NC_FLOAT: mss_val_flt=*mss_val.fp; break;
    case NC_DOUBLE: mss_val_dbl=*mss_val.dp; break;
    case NC_SHORT: mss_val_sht=*mss_val.sp; break;
    case NC_INT: mss_val_lng=*mss_val.lp; break;
    case NC_BYTE: mss_val_byt=*mss_val.bp; break;
    case NC_CHAR: mss_val_chr=*mss_val.cp; break;
    default: nco_dfl_case_nc_type_err(); break;
    } /* end switch */
  } /* endif */

  switch(type){
  case NC_FLOAT:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){ 
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	/* Operations: 1 multiply 
	   Repetitions: \dmnszavg^(\dmnnbr-\avgnbr)
	   Total Counts: \ntgnbr=\dmnszavg^(\dmnnbr-\avgnbr) */
	const long blk_off=idx_op2*sz_blk;
	/* Operations: 1 fp add, 3 pointer offsets, 3 user memory fetch
	   Repetitions: \lmnnbr
	   Total Counts: \flpnbr=\lmnnbr, \ntgnbr=3\lmnnbr, \mmrusrnbr=3\lmnnbr,
	   NB: Counted LHS+RHS+tally offsets and fetches */
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.fp[idx_op2]+=op1.fp[blk_off+idx_blk];
	tally[idx_op2]=sz_blk;
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.fp[idx_op1] != mss_val_flt){
	    op2.fp[idx_op2]+=op1.fp[idx_op1];
	    tally[idx_op2]++;
	  } /* end if */
	} /* end loop over idx_blk */
	if(tally[idx_op2] == 0L) op2.fp[idx_op2]=mss_val_flt;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      float op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.fp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.fp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	  tally[idx_op2]=sz_blk;
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_flt){
	      op2.fp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	      tally[idx_op2]++;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(tally[idx_op2] == 0L) op2.fp[idx_op2]=mss_val_flt;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_DOUBLE:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.dp[idx_op2]+=op1.dp[blk_off+idx_blk];
	tally[idx_op2]=sz_blk;
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.dp[idx_op1] != mss_val_dbl){
	    op2.dp[idx_op2]+=op1.dp[idx_op1];
	    tally[idx_op2]++;
	  } /* end if */
	} /* end loop over idx_blk */
	if(tally[idx_op2] == 0L) op2.dp[idx_op2]=mss_val_dbl;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      double op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.dp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.dp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	  tally[idx_op2]=sz_blk;
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_dbl){
	      op2.dp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	      tally[idx_op2]++;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(tally[idx_op2] == 0L) op2.dp[idx_op2]=mss_val_dbl;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_INT:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.lp[idx_op2]+=op1.lp[blk_off+idx_blk];
	tally[idx_op2]=sz_blk;
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.lp[idx_op1] != mss_val_lng){
	    op2.lp[idx_op2]+=op1.lp[idx_op1];
	    tally[idx_op2]++;
	  } /* end if */
	} /* end loop over idx_blk */
	if(tally[idx_op2] == 0L) op2.lp[idx_op2]=mss_val_lng;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      long op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.lp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.lp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	  tally[idx_op2]=sz_blk;
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_lng){
	      op2.lp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	      tally[idx_op2]++;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(tally[idx_op2] == 0L) op2.lp[idx_op2]=mss_val_lng;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_SHORT:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.sp[idx_op2]+=op1.sp[blk_off+idx_blk];
	tally[idx_op2]=sz_blk;
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.sp[idx_op1] != mss_val_sht){
	    op2.sp[idx_op2]+=op1.sp[idx_op1];
	    tally[idx_op2]++;
	  } /* end if */
	} /* end loop over idx_blk */
	if(tally[idx_op2] == 0L) op2.sp[idx_op2]=mss_val_sht;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      short op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.sp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++) op2.sp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	  tally[idx_op2]=sz_blk;
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_sht){
	      op2.sp[idx_op2]+=op1_2D[idx_op2][idx_blk];
	      tally[idx_op2]++;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(tally[idx_op2] == 0L) op2.sp[idx_op2]=mss_val_sht;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_CHAR:
    /* Do nothing except avoid compiler warnings */
    mss_val_chr=mss_val_chr;
    break;
  case NC_BYTE:
    /* Do nothing except avoid compiler warnings */
    mss_val_byt=mss_val_byt;
    break;
  default: nco_dfl_case_nc_type_err(); break;
  } /* end switch */
  
  /* NB: it is not neccessary to un-typecast pointers to values after access 
     because we have only operated on local copies of them. */

} /* end nco_var_avg_reduce_ttl() */

void
nco_var_avg_reduce_min /* [fnc] Place minimum of op1 blocks into each element of op2 */
(const nc_type type, /* I [enm] netCDF type of operands */
 const long sz_op1, /* I [nbr] Size (in elements) of op1 */
 const long sz_op2, /* I [nbr] Size (in elements) of op2 */
 const int has_mss_val, /* I [flg] Operand has missing values */
 ptr_unn mss_val, /* I [sct] Missing value */
 ptr_unn op1, /* I [sct] Operand (sz_op2 contiguous blocks of size (sz_op1/sz_op2)) */
 ptr_unn op2) /* O [sct] Minimum of each block of op1 */
{
  /* Purpose: Find minimum value of each contiguous block of first operand and place
     result in corresponding element in second operand. Operands are assumed to have
     conforming types, but not dimensions or sizes. */

  /* nco_var_avg_reduce_min() is derived from nco_var_avg_reduce_ttl()
     Routines are very similar but tallies are not incremented
     See nco_var_avg_reduce_ttl() for more algorithmic documentation
     nco_var_avg_reduce_max() is derived from nco_var_avg_reduce_min() */

#define FXM_NCO315 1
#ifdef FXM_NCO315
  long idx_op1;
#endif /* __GNUC__ */
  const long sz_blk=sz_op1/sz_op2;
  long idx_op2;
  long idx_blk;

  double mss_val_dbl=double_CEWI;
  float mss_val_flt=float_CEWI;
  nco_int mss_val_lng=nco_int_CEWI;
  short mss_val_sht=short_CEWI;
  nco_char mss_val_chr;
  nco_byte mss_val_byt;
  
  bool flg_mss=False; /* [flg] Block has valid (non-missing) values */
  
  /* Typecast pointer to values before access */
  (void)cast_void_nctype(type,&op1);
  (void)cast_void_nctype(type,&op2);
  if(has_mss_val) (void)cast_void_nctype(type,&mss_val);
  
  if(has_mss_val){
    switch(type){
    case NC_FLOAT: mss_val_flt=*mss_val.fp; break;
    case NC_DOUBLE: mss_val_dbl=*mss_val.dp; break;
    case NC_SHORT: mss_val_sht=*mss_val.sp; break;
    case NC_INT: mss_val_lng=*mss_val.lp; break;
    case NC_BYTE: mss_val_byt=*mss_val.bp; break;
    case NC_CHAR: mss_val_chr=*mss_val.cp; break;
    default: nco_dfl_case_nc_type_err(); break;
    } /* end switch */
  } /* endif */
  
  switch(type){
  case NC_FLOAT:
    
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){ 
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.fp[idx_op2]=op1.fp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.fp[idx_op2] > op1.fp[blk_off+idx_blk]) op2.fp[idx_op2]=op1.fp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.fp[idx_op1] != mss_val_flt) {
	    if(!flg_mss || op2.fp[idx_op2] > op1.fp[idx_op1]) op2.fp[idx_op2]=op1.fp[idx_op1];
	    flg_mss=True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.fp[idx_op2]=mss_val_flt;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      float op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.fp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.fp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.fp[idx_op2] > op1_2D[idx_op2][idx_blk]) op2.fp[idx_op2]=op1_2D[idx_op2][idx_blk];
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_flt) {
	      if(!flg_mss || op2.fp[idx_op2] > op1_2D[idx_op2][idx_blk]) op2.fp[idx_op2]=op1_2D[idx_op2][idx_blk];
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.fp[idx_op2]=mss_val_flt;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    
    break;
  case NC_DOUBLE:
    
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.dp[idx_op2]=op1.dp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.dp[idx_op2] > op1.dp[blk_off+idx_blk]) op2.dp[idx_op2]=op1.dp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.dp[idx_op1] != mss_val_dbl) {
	    if(!flg_mss || (op2.dp[idx_op2] > op1.dp[idx_op1])) op2.dp[idx_op2]=op1.dp[idx_op1];
	    flg_mss=True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.dp[idx_op2]=mss_val_dbl;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      double op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.dp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.dp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.dp[idx_op2] > op1_2D[idx_op2][idx_blk]) op2.dp[idx_op2]=op1_2D[idx_op2][idx_blk] ;
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_dbl){
	      if(!flg_mss || (op2.dp[idx_op2] > op1_2D[idx_op2][idx_blk])) op2.dp[idx_op2]=op1_2D[idx_op2][idx_blk];	    
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.dp[idx_op2]=mss_val_dbl;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_INT:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.lp[idx_op2]=op1.lp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.lp[idx_op2] > op1.lp[blk_off+idx_blk]) op2.lp[idx_op2]=op1.lp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.lp[idx_op1] != mss_val_lng){
	    if(!flg_mss || op2.lp[idx_op2] > op1.lp[idx_op1]) op2.lp[idx_op2]=op1.lp[idx_op1];
	    flg_mss= True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.lp[idx_op2]=mss_val_lng;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      long op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.lp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.lp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.lp[idx_op2] > op1_2D[idx_op2][idx_blk]) op2.lp[idx_op2]=op1_2D[idx_op2][idx_blk];
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_lng){
	      if(!flg_mss || op2.lp[idx_op2] > op1_2D[idx_op2][idx_blk]) op2.lp[idx_op2]=op1_2D[idx_op2][idx_blk];	      
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.lp[idx_op2]=mss_val_lng;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_SHORT:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.sp[idx_op2]=op1.sp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.sp[idx_op2] > op1.sp[blk_off+idx_blk]) op2.sp[idx_op2]=op1.sp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.sp[idx_op1] != mss_val_sht){
	    if(!flg_mss || op2.sp[idx_op2] > op1.sp[idx_op1]) op2.sp[idx_op2]=op1.sp[idx_op1];
	    flg_mss=True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.sp[idx_op2]=mss_val_sht;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      short op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.sp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.sp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.sp[idx_op2] > op1_2D[idx_op2][idx_blk]) op2.sp[idx_op2]=op1_2D[idx_op2][idx_blk];
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_sht){
	      if(!flg_mss  || op2.sp[idx_op2] > op1_2D[idx_op2][idx_blk]) op2.sp[idx_op2]=op1_2D[idx_op2][idx_blk];	      
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.sp[idx_op2]=mss_val_sht;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_CHAR:
    /* Do nothing except avoid compiler warnings */
    mss_val_chr=mss_val_chr;
    break;
  case NC_BYTE:
    /* Do nothing except avoid compiler warnings */
    mss_val_byt=mss_val_byt;
    break;
  default: nco_dfl_case_nc_type_err(); break;
  } /* end  switch */
  
  /* NB: it is not neccessary to un-typecast pointers to values after access 
     because we have only operated on local copies of them. */
  
} /* end nco_var_avg_reduce_min() */

void
nco_var_avg_reduce_max /* [fnc] Place maximum of op1 blocks into each element of op2 */
(const nc_type type, /* I [enm] netCDF type of operands */
 const long sz_op1, /* I [nbr] Size (in elements) of op1 */
 const long sz_op2, /* I [nbr] Size (in elements) of op2 */
 const int has_mss_val, /* I [flg] Operand has missing values */
 ptr_unn mss_val, /* I [sct] Missing value */
 ptr_unn op1, /* I [sct] Operand (sz_op2 contiguous blocks of size (sz_op1/sz_op2)) */
 ptr_unn op2) /* O [sct] Maximum of each block of op1 */
{
  /* Purpose: Find maximum value of each contiguous block of first operand and place
     result in corresponding element in second operand. Operands are assumed to have
     conforming types, but not dimensions or sizes. */

  /* nco_var_avg_reduce_min() is derived from nco_var_avg_reduce_ttl()
     Routines are very similar but tallies are not incremented
     See nco_var_avg_reduce_ttl() for more algorithmic documentation
     nco_var_avg_reduce_max() is derived from nco_var_avg_reduce_min() */

#define FXM_NCO315 1
#ifdef FXM_NCO315
  long idx_op1;
#endif /* __GNUC__ */
  const long sz_blk=sz_op1/sz_op2;
  long idx_op2;
  long idx_blk;
  
  double mss_val_dbl=double_CEWI;
  float mss_val_flt=float_CEWI;
  nco_int mss_val_lng=nco_int_CEWI;
  short mss_val_sht=short_CEWI;
  nco_char mss_val_chr;
  nco_byte mss_val_byt;
  
  bool flg_mss=False;
  
  /* Typecast pointer to values before access */
  (void)cast_void_nctype(type,&op1);
  (void)cast_void_nctype(type,&op2);
  if(has_mss_val) (void)cast_void_nctype(type,&mss_val);
  
  if(has_mss_val){
    switch(type){
    case NC_FLOAT: mss_val_flt=*mss_val.fp; break;
    case NC_DOUBLE: mss_val_dbl=*mss_val.dp; break;
    case NC_SHORT: mss_val_sht=*mss_val.sp; break;
    case NC_INT: mss_val_lng=*mss_val.lp; break;
    case NC_BYTE: mss_val_byt=*mss_val.bp; break;
    case NC_CHAR: mss_val_chr=*mss_val.cp; break;
    default: nco_dfl_case_nc_type_err(); break;
    } /* end switch */
  } /* endif */
  
  switch(type){
  case NC_FLOAT:
    
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){ 
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.fp[idx_op2]=op1.fp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.fp[idx_op2] < op1.fp[blk_off+idx_blk]) op2.fp[idx_op2]=op1.fp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.fp[idx_op1] != mss_val_flt) {
	    if(!flg_mss || op2.fp[idx_op2] < op1.fp[idx_op1]) op2.fp[idx_op2]=op1.fp[idx_op1];
	    flg_mss=True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.fp[idx_op2]=mss_val_flt;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      float op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.fp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.fp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.fp[idx_op2] < op1_2D[idx_op2][idx_blk]) op2.fp[idx_op2]=op1_2D[idx_op2][idx_blk];
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_flt) {
	      if(!flg_mss || op2.fp[idx_op2] < op1_2D[idx_op2][idx_blk]) op2.fp[idx_op2]=op1_2D[idx_op2][idx_blk];
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.fp[idx_op2]=mss_val_flt;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    
    break;
  case NC_DOUBLE:
    
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.dp[idx_op2]=op1.dp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.dp[idx_op2] < op1.dp[blk_off+idx_blk]) op2.dp[idx_op2]=op1.dp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.dp[idx_op1] != mss_val_dbl) {
	    if(!flg_mss || (op2.dp[idx_op2] < op1.dp[idx_op1])) op2.dp[idx_op2]=op1.dp[idx_op1];
	    flg_mss=True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.dp[idx_op2]=mss_val_dbl;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      double op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.dp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.dp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.dp[idx_op2] < op1_2D[idx_op2][idx_blk]) op2.dp[idx_op2]=op1_2D[idx_op2][idx_blk] ;
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_dbl){
	      if(!flg_mss || (op2.dp[idx_op2] < op1_2D[idx_op2][idx_blk])) op2.dp[idx_op2]=op1_2D[idx_op2][idx_blk];	    
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.dp[idx_op2]=mss_val_dbl;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_INT:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.lp[idx_op2]=op1.lp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.lp[idx_op2] < op1.lp[blk_off+idx_blk]) op2.lp[idx_op2]=op1.lp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.lp[idx_op1] != mss_val_lng){
	    if(!flg_mss || op2.lp[idx_op2] < op1.lp[idx_op1]) op2.lp[idx_op2]=op1.lp[idx_op1];
	    flg_mss= True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.lp[idx_op2]=mss_val_lng;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      long op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.lp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.lp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.lp[idx_op2] < op1_2D[idx_op2][idx_blk]) op2.lp[idx_op2]=op1_2D[idx_op2][idx_blk];
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_lng){
	      if(!flg_mss || op2.lp[idx_op2] < op1_2D[idx_op2][idx_blk]) op2.lp[idx_op2]=op1_2D[idx_op2][idx_blk];	      
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.lp[idx_op2]=mss_val_lng;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_SHORT:
#define FXM_NCO315 1
#ifdef FXM_NCO315
    /* ANSI-compliant branch */
    if(!has_mss_val){
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	op2.sp[idx_op2]=op1.sp[blk_off];
	for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	  if(op2.sp[idx_op2] < op1.sp[blk_off+idx_blk]) op2.sp[idx_op2]=op1.sp[blk_off+idx_blk];
      } /* end loop over idx_op2 */
    }else{
      for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	const long blk_off=idx_op2*sz_blk;
	flg_mss=False;
	for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	  idx_op1=blk_off+idx_blk;
	  if(op1.sp[idx_op1] != mss_val_sht){
	    if(!flg_mss || op2.sp[idx_op2] < op1.sp[idx_op1]) op2.sp[idx_op2]=op1.sp[idx_op1];
	    flg_mss=True;
	  } /* end if */
	} /* end loop over idx_blk */
	if(!flg_mss) op2.sp[idx_op2]=mss_val_sht;
      } /* end loop over idx_op2 */
    } /* end else */
#else /* __GNUC__ */
    /* Compiler supports local automatic arrays. Not ANSI-compliant, but more elegant. */
    if(True){
      short op1_2D[sz_op2][sz_blk];
      
      (void)memcpy((void *)op1_2D,(void *)(op1.sp),sz_op1*nco_typ_lng(type));
      
      if(!has_mss_val){
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  op2.sp[idx_op2]=op1_2D[idx_op2][0];
	  for(idx_blk=1;idx_blk<sz_blk;idx_blk++) 
	    if(op2.sp[idx_op2] < op1_2D[idx_op2][idx_blk]) op2.sp[idx_op2]=op1_2D[idx_op2][idx_blk];
	} /* end loop over idx_op2 */
      }else{
	for(idx_op2=0;idx_op2<sz_op2;idx_op2++){
	  flg_mss=False;
	  for(idx_blk=0;idx_blk<sz_blk;idx_blk++){
	    if(op1_2D[idx_op2][idx_blk] != mss_val_sht){
	      if(!flg_mss  || op2.sp[idx_op2] < op1_2D[idx_op2][idx_blk]) op2.sp[idx_op2]=op1_2D[idx_op2][idx_blk];	      
	      flg_mss=True;
	    } /* end if */
	  } /* end loop over idx_blk */
	  if(!flg_mss) op2.sp[idx_op2]=mss_val_sht;
	} /* end loop over idx_op2 */
      } /* end else */
    } /* end if */
#endif /* __GNUC__ */
    break;
  case NC_CHAR:
    /* Do nothing except avoid compiler warnings */
    mss_val_chr=mss_val_chr;
    break;
  case NC_BYTE:
    /* Do nothing except avoid compiler warnings */
    mss_val_byt=mss_val_byt;
    break;
  default: nco_dfl_case_nc_type_err(); break;
  } /* end  switch */
  
  /* NB: it is not neccessary to un-typecast pointers to values after access 
     because we have only operated on local copies of them. */

} /* end nco_var_avg_reduce_max() */
