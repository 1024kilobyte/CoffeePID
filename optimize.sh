#!/bin/sh

gzip='gzip -k --force'
purgecss='purgecss --content *.html js/*.js --output css_prg'

optimize() {
  file="$1".min.css
  [ ! -f css/"$file" ] && file="$1".css
  opt_file="$1".opt.css

  $purgecss --css css/"$file"
  uglifycss css_prg/"$file" > css_prg/"$opt_file"
  $gzip css_prg/"$opt_file"
  mv -f css_prg/"$opt_file" css
  mv -f css_prg/"$opt_file".gz css
}

minify() {
  file=js/"$1".js
  min_file=js/"$1".min.js

  uglifyjs "$file" > "$min_file"
  $gzip "$min_file"
}


cd data/www
# purge css, minify, gzip
mkdir css_prg
optimize bs4
optimize chartist
optimize app
rm -rf css_prg

# minify all js files
minify app
#minify content
minify ws
$gzip js/content.js

# zip all html files
$gzip *.html