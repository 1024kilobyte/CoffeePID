#!/bin/sh
cd data/www
# purge css, minify, gzip
mkdir css_prg
purgecss --css css/bs4.min.css --content '*.html','js/*.js' --output css_prg
purgecss --css css/app.css --content '*.html','js/*.js' --output css_prg
uglifycss css_prg/bs4.min.css > css_prg/bs4.opt.css
uglifycss css_prg/app.css > css_prg/app.opt.css
gzip css_prg/bs4.opt.css
gzip css_prg/app.opt.css
mv -f css_prg/bs4.opt.css.gz css/bs4.opt.css.gz
mv -f css_prg/app.opt.css.gz css/app.opt.css.gz
rm -rf css_prg
# minify all js files
uglifyjs js/app.js > js/app.min.js
gzip --force js/app.min.js
# uglifyjs js/content.js > js/content.min.js
gzip --force js/content.js
# zip all html files
gzip --force *.html