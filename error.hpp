// frea: a5f44c70187b9ba310575c5084642b5e5fbd6b2f からのコピー
#pragma once
#include <iostream>

#define SOURCEPOS ::frea::SourcePos{__FILE__, __PRETTY_FUNCTION__, __func__, __LINE__}
#ifdef DEBUG
	#define D_Expect(exp, msg)	if(!(exp)) { std::cerr << "---<except macro failed!>---" << std::endl << SOURCEPOS << "message:\t" << msg << std::endl; }
#else
	#define D_Expect(...) {}
#endif

namespace frea {
	//! ソースコード上の位置を表す情報
	struct SourcePos {
		const char	*filename,
					*funcname,
					*funcname_short;
		int			line;
	};
	inline std::ostream& operator << (std::ostream& s, const SourcePos& p) {
		using std::endl;
		return s
			<< "at file:\t" << p.filename << endl
			<< "at function:\t" << p.funcname << endl
			<< "on line:\t" << p.line << endl;
	}
}
