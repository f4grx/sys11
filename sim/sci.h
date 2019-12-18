#ifndef __sci__h__
#define __sci__h__

struct hc11_sci;

struct hc11_sci* hc11_sci_init(struct hc11_core *core);
void hc11_sci_close(struct hc11_sci *sci);

#endif /* __sci__h__ */
