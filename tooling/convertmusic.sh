# old: raw pcm
#ffmpeg -y -i $1 temp.wav
#sox temp.wav -c 1 -r 32000 music.raw
#mv music.raw $2
#rm temp.wav

# much better: ogg
ffmpeg -n -i $1 -ar 32000 -ab 192k $2
#cp $1 $2
echo "#define MUSIC_LEN_SEC $(ffprobe -i $2 -show_entries format=duration -v quiet -of csv="p=0")" > $3
