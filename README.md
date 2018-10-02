# ray.sniff-

convert our web field

[![IMAGE ALT TEXT HERE](http://img.youtube.com/vi/EsE90BRTuCc/0.jpg)](http://www.youtube.com/watch?v=EsE90BRTuCc)

# Project Page
(http://cycling74.com/project/ray-sniff-record-our-web-field/)

# Concept 

At first, I thought that I want to create a system that compose a music automatically when dive into the Web.
Now, I am interested in sounds of network data, inspired Alva noto’s work “unitxt”. so I create ray.sniff~.

> after the first ten tracks that can be regarded as the core recordings of unitxt,
> there are 15 more tracks generated from converting pure data of programs, jpgs or other digital files into sound material. 
> these tracks could be considered as source code ‘solos’ to be played on top of the first ten tracks or to be regarded as sonified concept recordings.
> Raster-noton – Alva Noto/unitxt(http://www.raster-noton.net/main.php?action=products&dat=146)

ray.sniff~ is a max external object that is converting Web data to signal data. I create it using libpcap(http://www.tcpdump.org/).
When we access to any sites or upload any files, those data make sounds. ray.sniff~ enable us to convert innumerable data to sound.
If you dive to the Web with ray.sniff~, you have a special experience. You can dive to find exciting sounds deeper than ever.
ray.sniff~ invites you to new relationship with the Web as amazing sound source.
Now this object is beta, has many points of improvement, such as

* stability
* need administration when run Max.app. Now I use adminlaunch(http://sourceforge.net/projects/macosxutils/files/adminlaunch/)
* only Mac
* dirty code
* could not completely sniff a network action
