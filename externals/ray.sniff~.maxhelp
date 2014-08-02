{
	"patcher" : 	{
		"fileversion" : 1,
		"rect" : [ 378.0, 186.0, 640.0, 506.0 ],
		"bglocked" : 0,
		"defrect" : [ 378.0, 186.0, 640.0, 506.0 ],
		"openrect" : [ 0.0, 0.0, 0.0, 0.0 ],
		"openinpresentation" : 0,
		"default_fontsize" : 10.0,
		"default_fontface" : 0,
		"default_fontname" : "Verdana",
		"gridonopen" : 0,
		"gridsize" : [ 5.0, 5.0 ],
		"gridsnaponopen" : 0,
		"toolbarvisible" : 1,
		"boxanimatetime" : 200,
		"imprint" : 0,
		"enablehscroll" : 1,
		"enablevscroll" : 1,
		"devicewidth" : 0.0,
		"boxes" : [ 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "Clear the buffer",
					"numoutlets" : 0,
					"presentation_rect" : [ 299.0, 177.0, 0.0, 0.0 ],
					"id" : "obj-6",
					"fontname" : "Verdana",
					"patching_rect" : [ 300.0, 180.0, 89.0, 19.0 ],
					"fontsize" : 10.0,
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "message",
					"text" : "clear",
					"numoutlets" : 1,
					"id" : "obj-3",
					"fontname" : "Verdana",
					"outlettype" : [ "" ],
					"patching_rect" : [ 360.0, 210.0, 35.0, 17.0 ],
					"fontsize" : 10.0,
					"numinlets" : 2
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "arguments\n\nmust:\n1 : Bit Depth on sampling\n          you can choose\n          8, 16, 24 and 32\n\nany:\n2 : Network device name\n         check on terminal\n         to type > sudo ifconfig\n\n3 : Monitoring IP Address\n         maybe cant use xxx:xxx:xxx:xxx style\n\n4 : Monitoring Subnet\n         Also IP address\n\n5 - : packet filtering\n           ex.)monitoring port 80\n                   port 80",
					"linecount" : 21,
					"numoutlets" : 0,
					"id" : "obj-36",
					"fontname" : "Verdana",
					"patching_rect" : [ 35.0, 170.0, 242.0, 262.0 ],
					"fontsize" : 10.0,
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "Glitch switch \nplease try it",
					"linecount" : 2,
					"numoutlets" : 0,
					"id" : "obj-34",
					"fontname" : "Verdana",
					"patching_rect" : [ 455.0, 190.0, 144.0, 31.0 ],
					"fontsize" : 10.0,
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"text" : "Start Sampling",
					"numoutlets" : 0,
					"id" : "obj-33",
					"fontname" : "Verdana",
					"patching_rect" : [ 425.0, 165.0, 89.0, 19.0 ],
					"fontsize" : 10.0,
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "message",
					"text" : "glitch $1",
					"numoutlets" : 1,
					"id" : "obj-32",
					"fontname" : "Verdana",
					"outlettype" : [ "" ],
					"patching_rect" : [ 430.0, 220.0, 54.0, 17.0 ],
					"fontsize" : 10.0,
					"numinlets" : 2
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "led",
					"numoutlets" : 1,
					"id" : "obj-29",
					"outlettype" : [ "int" ],
					"patching_rect" : [ 430.0, 190.0, 20.0, 20.0 ],
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "led",
					"numoutlets" : 1,
					"id" : "obj-28",
					"outlettype" : [ "int" ],
					"patching_rect" : [ 400.0, 165.0, 20.0, 20.0 ],
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "loadmess 128",
					"numoutlets" : 1,
					"id" : "obj-23",
					"fontname" : "Verdana",
					"outlettype" : [ "" ],
					"hidden" : 1,
					"patching_rect" : [ 495.0, 250.0, 80.0, 19.0 ],
					"fontsize" : 10.0,
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "gain~",
					"numoutlets" : 2,
					"id" : "obj-22",
					"outlettype" : [ "signal", "int" ],
					"patching_rect" : [ 400.0, 290.0, 178.0, 21.0 ],
					"numinlets" : 2
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "ezdac~",
					"numoutlets" : 0,
					"id" : "obj-21",
					"patching_rect" : [ 400.0, 340.0, 45.0, 45.0 ],
					"numinlets" : 2
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"varname" : "autohelp_top_title",
					"text" : "ray.sniff~",
					"numoutlets" : 0,
					"id" : "obj-2",
					"fontname" : "Osaka",
					"patching_rect" : [ 25.0, 23.0, 485.0, 36.0 ],
					"frgb" : [ 0.93, 0.93, 0.97, 1.0 ],
					"fontface" : 3,
					"fontsize" : 20.871338,
					"textcolor" : [ 0.93, 0.93, 0.97, 1.0 ],
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"varname" : "autohelp_top_digest",
					"text" : "ネットでの活動をサンプリングする / Sampling your net activity",
					"numoutlets" : 0,
					"id" : "obj-4",
					"fontname" : "Osaka",
					"patching_rect" : [ 25.0, 50.0, 485.0, 25.0 ],
					"frgb" : [ 0.93, 0.93, 0.97, 1.0 ],
					"fontsize" : 12.754705,
					"textcolor" : [ 0.93, 0.93, 0.97, 1.0 ],
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "comment",
					"varname" : "autohelp_top_description",
					"text" : "ray.sniff~\nはウェブブラウジング中のデータをリアルタイムサンプリングする。\nray.sniff~\nRealtime sampling your net activity",
					"linecount" : 4,
					"numoutlets" : 0,
					"id" : "obj-5",
					"fontname" : "Osaka",
					"patching_rect" : [ 25.0, 77.0, 458.0, 72.0 ],
					"fontsize" : 11.595187,
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "newobj",
					"text" : "ray.sniff~ 16",
					"numoutlets" : 1,
					"id" : "obj-1",
					"fontname" : "Verdana",
					"outlettype" : [ "signal" ],
					"patching_rect" : [ 400.0, 245.0, 75.0, 19.0 ],
					"fontsize" : 10.0,
					"numinlets" : 1
				}

			}
, 			{
				"box" : 				{
					"maxclass" : "panel",
					"varname" : "autohelp_top_panel",
					"numoutlets" : 0,
					"id" : "obj-18",
					"patching_rect" : [ 20.0, 20.0, 495.0, 52.0 ],
					"grad1" : [ 0.403922, 0.109804, 0.701961, 1.0 ],
					"mode" : 1,
					"grad2" : [ 1.0, 1.0, 1.0, 1.0 ],
					"numinlets" : 1,
					"background" : 1
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"source" : [ "obj-3", 0 ],
					"destination" : [ "obj-1", 0 ],
					"hidden" : 0,
					"midpoints" : [  ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-23", 0 ],
					"destination" : [ "obj-22", 0 ],
					"hidden" : 1,
					"midpoints" : [  ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-29", 0 ],
					"destination" : [ "obj-32", 0 ],
					"hidden" : 0,
					"midpoints" : [  ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-22", 0 ],
					"destination" : [ "obj-21", 0 ],
					"hidden" : 0,
					"midpoints" : [  ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-22", 0 ],
					"destination" : [ "obj-21", 1 ],
					"hidden" : 0,
					"midpoints" : [  ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-32", 0 ],
					"destination" : [ "obj-1", 0 ],
					"hidden" : 0,
					"midpoints" : [  ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-28", 0 ],
					"destination" : [ "obj-1", 0 ],
					"hidden" : 0,
					"midpoints" : [  ]
				}

			}
, 			{
				"patchline" : 				{
					"source" : [ "obj-1", 0 ],
					"destination" : [ "obj-22", 0 ],
					"hidden" : 0,
					"midpoints" : [  ]
				}

			}
 ]
	}

}
