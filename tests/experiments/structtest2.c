#include <stdlib.h>

struct model
{
	double x, y, w, h;
};

int main (int argc,char * * const argv)
{
	int _lc_tmp = ({
		struct model const * const m = ({
			struct model * _lc_tmp0 = (struct model *) malloc (sizeof (unsigned) + sizeof (struct model));
			{
				_lc_tmp0 = (struct model *) (((unsigned *) _lc_tmp0) + 1);
				((unsigned *) _lc_tmp0)[-1] = 1;
				*_lc_tmp0 = (struct model) {.x=0.0,.y=0.0,.w=1.0,.h=1.0,};
			};
			_lc_tmp0;
		});
		m;
	});
	return 0;
}
