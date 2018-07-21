/*	$OpenBSD: funmap.h,v 1.6 2005/11/18 20:56:52 deraadt Exp $	*/

/* This file is in the public domain */

void		 funmap_init(void);
PF		 name_function(const char *);
const char	*function_name(PF);
struct list	*complete_function_list(const char *);
int		 funmap_add(PF, const char *);
