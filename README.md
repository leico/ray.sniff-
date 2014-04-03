ray.sniff-
==========

record our web field

[![IMAGE ALT TEXT HERE](http://img.youtube.com/vi/EsE90BRTuCc/0.jpg)](http://www.youtube.com/watch?v=EsE90BRTuCc)

Project Page
-----------
(http://cycling74.com/project/ray-sniff-record-our-web-field/)

English
-----------
At first, I thought that I want to create a system that compose a music automatically when dive into the Web.
Now, I am interested in sounds of network data, inspired Alva noto's work "unitxt". so I create ray.sniff~.

> after the first ten tracks that can be regarded as the core recordings of unitxt, there are 15 more tracks generated from converting pure data of programs, jpgs or other digital files into sound material. these tracks could be considered as source code 'solos' to be played on top of the first ten tracks or to be regarded as sonified concept recordings.
>
> Raster-noton - Alva Noto/unitxt(http://www.raster-noton.net/main.php?action=products&amp;dat=146)

ray.sniff~ is a max external object that is converting Web data to signal data. I create it using libpcap(http://www.tcpdump.org/).

When we access to any sites or upload any files, those data make sounds. ray.sniff~ enable us to convert innumerable data to sound.

If you dive to the Web with ray.sniff~, you have a special experience. You will dive deeper than usual times for to find good sounds. You will perceive you see the Web as sound source.

Also, You will know raw data is rich contents. ray.sniff~ indicate a another view point of raw data.

Now this object is beta, has many points of improvement, such as

* stability
* need administration when run Max.app. Now I use adminlaunch(http://sourceforge.net/projects/macosxutils/files/adminlaunch/)
* only Mac
* dirty code
* could not completely sniff a network action

now we can convert the Web data to sound but only.

In the future, I try 'pure data' sonificate of other network.


-----------------------------------------------------
日本語
-----------------

私は最初、Webサーフィンしていたら自動的に曲ができるものが欲しいと思っていました。
しかしAlva Noto "unitxt"を知った今、私はネットワークデータが持つ音に興味があり、ray.sniff~オブジェクトを製作しました。

> after the first ten tracks that can be regarded as the core recordings of unitxt, there are 15 more tracks generated from converting pure data of programs, jpgs or other digital files into sound material. these tracks could be considered as source code 'solos' to be played on top of the first ten tracks or to be regarded as sonified concept recordings.
>
> Raster-noton - Alva Noto/unitxt(http://www.raster-noton.net/main.php?action=products&amp;dat=146)

ray.sniff~はWebのデータをシグナルデータに変換するエクスターナルオブジェクトです。libpcap(http://www.tcpdump.org/) を使って作りました。

どこかのサイトにアクセスしたり、ファイルをアップロードしたりすると、そのデータを音に変換します。私たちは無限に近いデータを音に変換できるようになりました。

もしray.sniff~を使いながらWebにもぐった時、いつもより深くまで、いつも行かないところまで、よい音を探すことになるでしょう。その時Webを音源として知覚している自分に気づくはずです。

また、Rawデータの持つ豊かさ、及びデータを使う別の視点を見出すことができるでしょう。

このオブジェクトはまだ開発途上です。以下のようないくつもの改善点があります。

* 安定性
* Max立ち上げにAdmin権限が必要。自分はAdminLaunch(http://sourceforge.net/projects/macosxutils/files/adminlaunch/) を使っています。
* Macのみ
* コードが汚い
* 完全に全てのデータを変換している訳ではない

このオブジェクトによって、私たちはWebのデータを音に変換することができるようになりましたが、しかしWebだけしか音に変えれません。

この先、他のネットワークを'pure data'可聴化する予定でいます。

update 2014/4/3
