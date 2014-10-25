#define SPARC_RET_VOID		0
#define SPARC_RET_STRUCT	1
#define SPARC_RET_UINT8		2
#define SPARC_RET_SINT8		3
#define SPARC_RET_UINT16	4
#define SPARC_RET_SINT16	5
#define SPARC_RET_UINT32	6
#define SPARC_RET_SINT32	7	/* v9 only */
#define SPARC_RET_INT64		8
#define SPARC_RET_INT128	9	/* v9 only */

/* Note that F_7 is missing, and is handled by SPARC_RET_STRUCT.  */
#define SPARC_RET_F_8		10
#define SPARC_RET_F_6		11	/* v9 only */
#define SPARC_RET_F_4		12
#define SPARC_RET_F_2		13
#define SPARC_RET_F_3		14	/* v9 only */
#define SPARC_RET_F_1		15

#define SPARC_FLAG_RET_MASK	15
#define SPARC_FLAG_RET_IN_MEM	32
#define SPARC_FLAG_FP_ARGS	64

#define SPARC_FLTMASK_SHIFT	8
