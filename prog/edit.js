
const fs = require('fs');
const path = require('path');

let fname = process.argv[2];
let name = fname.replace(/[.]c/, '').replace(/-/g, '_');

//console.log({argv: process.argv, fname})

let src = fs.readFileSync(fname, 'utf8');

if (! /BUILD_MONOLITHIC/.test(src)) {
	src = src.replace(/int main\(int/, function inject(m) {
		
		//console.log({m, name})
		return `
#if defined(BUILD_MONOLITHIC)
#define main   lept_${ name }_main
#endif

int main(int`;
	});
	src = src.replace(/l_int32 main\(int/, function inject(m) {
		
		//console.log({m, name})
		return `
#if defined(BUILD_MONOLITHIC)
#define main   lept_${ name }_main
#endif

int main(int`;
	});
	src = src.replace(/l_int32 main\(l_int32/, function inject(m) {
		
		//console.log({m, name})
		return `
#if defined(BUILD_MONOLITHIC)
#define main   lept_${ name }_main
#endif

int main(int`;
	});
}

if (! /monolithic_examples/.test(src)) {
	src = src.replace(/^([\s\S]*#include[^\n]*\n)([\s\S]*)$/, function inject(m, p1, p2) {
		
		//console.log({m, name})
		return `${p1}
#include "monolithic_examples.h"

${ p2 }`;
	});
}

if (/     char [*][*]argv/.test(src)) {
	src = src.replace(/     char [*][*]argv/, '     const char **argv');
}

fs.writeFileSync(fname, src, 'utf8');
