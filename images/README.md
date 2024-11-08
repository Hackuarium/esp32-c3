Source of images: https://opengameart.org/art-search-advanced?field_art_tags_tid=16x16

Crop images and transparent background (sometimes original is transparent we should not force then !!!)

`magick potions.png -crop 16x16 +repage  potions\_%d.gif`

`magick potions.png -crop 16x16 +repage -transparent white potions\_%d.gif`

Create an animaged gif

`magick potions.png -crop 16x16 +repage -transparent white +adjoin animated.gif`

// https://imagemagick.org/script/command-line-options.php?#adjoin
