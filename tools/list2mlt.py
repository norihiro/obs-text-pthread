#! /usr/bin/python3

import xml.etree.ElementTree as ET

def ms2str(t):
	ms = t%1000; t /= 1000
	s = t%60; t /= 60
	m = t%60; t /= 60
	h = t
	return '%d:%02d:%02d.%03d'%(h, m, s, ms)

_key = 1
class TPItem:
	def __init__(self, ms, png, w, h, fadein, crossfade, config):
		global _key
		self.ms_start = ms
		self.png = png
		self.w = w
		self.h = h
		self.ms_end = 0
		self.ms_end_fade = 0
		self.key = _key; _key += 1
		self.fadein = fadein
		self.crossfade = crossfade
		self.fadeout = False
		self.config = config
	def __str__(self):
		return 'TPItem(%s %s-%s %dx%d)'%(self.png, ms2str(self.ms_start), ms2str(self.ms_end), self.w, self.h)

def add_property(element, name, value):
	ET.SubElement(element, 'property', attrib={'name': name}).text = value

n_playlist = 0
class Playlist():
	def __init__(self):
		global n_playlist
		self.key = n_playlist; n_playlist += 1
		self.items = list()
	def __str__(self):
		return 'playlist%d'%self.key
	def add_item(self, item):
		self.items.append(item)
	def ms_end(self):
		if len(self.items)<=0:
			return 0
		return self.items[-1].ms_end_fade
	def add_to_mlt(self, mlt):
		playlist = ET.SubElement(mlt, 'playlist', attrib={'autoclose': '1', 'id': self.__str__()})
		ms_last = 0
		for item in self.items:
			if item.ms_start > ms_last:
				ET.SubElement(playlist, 'blank', attrib={'length': ms2str(item.ms_start - ms_last)})
			entry = ET.SubElement(playlist, 'entry', attrib={
				'producer': 'producer%d'%item.key,
				'in': ms2str(0),
				'out': ms2str(item.ms_end_fade - item.ms_start),
				})
			ms_last = item.ms_end_fade
			f1 = ET.SubElement(entry, 'filter', attrib={'id': 'filter%d'%item.key})
			x = (1920 - item.w)/2 # TODO: x
			y = 1060 - item.h # TODO: y
			rect_xywh = '%d %d %d %d'%(x, y, item.w, item.h)
			rects = list()
			fadein_ms = int(item.config['fadein_ms'])
			fadeout_ms = int(item.config['fadeout_ms'])
			fadeout_ms = int(int(fadeout_ms * 30 / 1000) * 1000 / 30)
			if fadein_ms>0 and item.fadein:
				rects.append('%s=%s 0.0'%(ms2str(0), rect_xywh))
				rects.append('%s=%s 1.0'%(ms2str(fadein_ms), rect_xywh))
			else:
				rects.append('%s=%s 1.0'%(ms2str(0), rect_xywh))
			if fadeout_ms>0 and item.fadeout:
				rects.append('%s=%s 1.0'%(ms2str(item.ms_end-item.ms_start), rect_xywh))
				rects.append('%s=%s 0.0'%(ms2str(item.ms_end_fade-item.ms_start), rect_xywh))
			add_property(f1, 'rotate_center', '1')
			add_property(f1, 'mlt_service', 'qtblend')
			add_property(f1, 'kdenlive_id', 'qtblend')
			add_property(f1, 'rect', ';'.join(rects))
			add_property(f1, 'rotation', '00:00:00.000=0')
			add_property(f1, 'compositing', '0')
			add_property(f1, 'distort', '0')
			add_property(f1, 'kdenlive:collapsed', '0')

class TP2Mlt:
	def push(self, item):
		self.items.append(item)
	def read_list(self, f):
		first = True
		self.items = list()
		item = None
		ms_first = None
		self.config = dict()
		while True:
			line = f.readline().strip('\r\n')
			if not line:
				break
			line = line.split('\t')
			if len(line) < 2:
				continue
			if line[0]=='#' and line[1][-1]==':':
				self.config = dict(self.config)
				self.config[line[1][:-1]] = line[2]
				continue
			ms = int(line[0])
			ms = int(int(ms*30/1000) * 1000/30)
			if not ms_first:
				ms_first = ms
				ms = 0
			else:
				ms -= ms_first
			png = line[1]
			is_crossfade = item and png!='-'
			is_fadein = not item and png!='-'
			is_fadeout = item and png=='-'
			if item:
				item.ms_end = ms
				if is_fadeout:
					item.fadeout = True
				self.push(item)
				item = None
			if png!='-' and len(line)>=4:
				item = TPItem(ms, png, int(line[2]), int(line[3]), is_fadein, is_crossfade, self.config)

	def make_xml(self):
		kdenlive = True
		mlt = ET.Element('mlt', attrib={'version': '6.22'})
		ET.SubElement(mlt, 'profile', attrib={
			'width': '1920',
			'height': '1080',
			'display_aspect_num': '16',
			'display_aspect_den': '9',
			'frame_rate_num': '30000',
			'frame_rate_den': '1001',
			'sample_aspect_num': '1',
			'sample_aspect_den': '1',
			'colorspace': '709',
			'progressive': '1',
			})
		fadeout_ms = 0
		for item in self.items:
			producer = ET.SubElement(mlt, 'producer', attrib={'id': 'producer%d'%item.key})
			add_property(producer, 'resource', item.png)
			add_property(producer, 'mlt_service', 'qimage')
			fadeout_ms = int(item.config['fadeout_ms'])
			fadeout_ms = int(int(fadeout_ms * 30 / 1000) * 1000 / 30)
			if fadeout_ms>0 and item.fadeout:
				item.ms_end_fade = item.ms_end + fadeout_ms
			else:
				item.ms_end_fade = item.ms_end
		if kdenlive:
			main_bin = ET.SubElement(mlt, 'playlist', attrib={'id': 'main_bin'})
			add_property(main_bin, 'xml_retain', '1')
			add_property(main_bin, 'kdenlive:docproperties.version', '1')
			for item in self.items:
				ET.SubElement(main_bin, 'entry', attrib={
					'producer': 'producer%d'%item.key,
					'in': ms2str(0),
					'out': ms2str(item.ms_end_fade - item.ms_start),
					})
		black_track = ET.SubElement(mlt, 'producer', attrib={
			'id': 'black_track',
			'in': ms2str(0),
			'out': ms2str(self.items[-1].ms_end_fade + fadeout_ms),
			})
		add_property(black_track, 'resource', 'transparent')
		add_property(black_track, 'mlt_service', 'color')
		playlists = list()
		for item in self.items:
			i_playlist = len(playlists)
			while i_playlist > 0:
				if playlists[i_playlist-1].ms_end() > item.ms_start:
					break
				i_playlist -= 1
			if i_playlist >= len(playlists):
				playlists.append(Playlist())
			playlists[i_playlist].add_item(item)
		for playlist in playlists:
			playlist.add_to_mlt(mlt)
			# kdenlive requires an enpty playlist at the end of each tractor
			playlist_e = Playlist()
			playlist_e.add_to_mlt(mlt)
			tractor = ET.SubElement(mlt, 'tractor', attrib={'id': 'tractor%d'%playlist.key})
			if kdenlive:
				add_property(tractor, 'kdenlive:trackheight', '61')
				add_property(tractor, 'kdenlive:timeline_active', '1')
			ET.SubElement(tractor, 'track', attrib={'hide': 'audio', 'producer': str(playlist)})
			ET.SubElement(tractor, 'track', attrib={'hide': 'both', 'producer': str(playlist_e)})
		global_feed = ET.SubElement(mlt, 'tractor', attrib={'id': 'tractor%d'%len(playlists), 'global_feed': '1'})
		ET.SubElement(global_feed, 'track', attrib={'producer': 'black_track'})
		for playlist in playlists:
			ET.SubElement(global_feed, 'track', attrib={'producer': 'tractor%d'%playlist.key})
		self.mlt = mlt
	def pretty(self):
		from xml.dom import minidom
		return minidom.parseString(ET.tostring(self.mlt)).toprettyxml()

def main():
	inst = TP2Mlt()
	import sys
	f = sys.argv[1]
	inst.read_list(open(f))
	inst.make_xml()
	print(inst.pretty())
	return 0

if __name__=='__main__':
	import sys
	sys.exit(main())

