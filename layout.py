#!/Applications/Kicad/kicad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python
import os
import sys
import math
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--delete-all-traces', action='store_true', help='Delete All Traces in the pcbnew and then exit')
parser.add_argument('--delete-short-traces', action='store_true', help='Delete traces of zero or very small length and exit')
parser.add_argument('--dry-run', action='store_true', help='Don\'t save results')
parser.add_argument('-v', dest='verbose', action='count', help="Verbose")
args = parser.parse_args()

sys.path.insert(0, "/Applications/Kicad/kicad.app/Contents/Frameworks/python/site-packages/")
sys.path.append("/Library/Python/2.7/site-packages")
sys.path.append("/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/site-packages")
os.chdir(os.path.dirname(os.path.realpath(__file__)))

pcb_path = "triangle.kicad_pcb"

import kicad
import pcbnew

from kicad.pcbnew import Board
board = Board.from_file(pcb_path)

layertable = {}

numlayers = pcbnew.PCB_LAYER_ID_COUNT
for i in range(numlayers):
	layertable[board._obj.GetLayerName(i)] = i

netcodes = board._obj.GetNetsByNetcode()
netnames = {}
for netcode, net in netcodes.items():
	netnames[net.GetNetname()] = net

def add_copper_trace(start, end, net):
	track = pcbnew.TRACK(board._obj)
	track.SetStart(start)
	track.SetEnd(end)
	track.SetLayer(layertable["F.Cu"])
	track.SetWidth(int(.25 * 10**6))
	track.SetNet(net)
	board._obj.Add(track)
	print("adding track from {} to {} on net {}".format(start, end, net.GetNetname()))

prev_module = None
def place(module, point, orientation = None):
	global prev_module
	module.position = point
	if orientation is not None:
		module._obj.SetOrientation(orientation)

	# Add tracks from the previous  module, connecting pad 5 to pad 2 and pad 4 to pad 3
	if prev_module is not None:
		pad_map = {"2": "5", "3": "4"} # connect SCK and SD*
		for prev_pad in prev_module._obj.Pads():
			if prev_pad.GetPadName() in pad_map:
				tracks = board._obj.TracksInNet(prev_pad.GetNet().GetNet())
				if tracks:
					# skip pad, already has traces
					if args.verbose:
						print("Skipping pad, already has tracks: {}".format(prev_pad))
					continue
				# for net in board._obj.TracksInNet(prev_pad.GetNet().GetNet()):
				# 	board._obj.Delete(t)

				# then connect the two pads
				for pad in module._obj.Pads():
					if pad.GetPadName() == pad_map[prev_pad.GetPadName()]:
						start = pcbnew.wxPoint(prev_pad.GetPosition().x, prev_pad.GetPosition().y)
						end = pcbnew.wxPoint(pad.GetPosition().x, pad.GetPosition().y)
						print("Adding track from module {} pad {} to module {} pad {}".format(prev_module.reference, prev_pad.GetPadName(), module.reference, pad.GetPadName()))
						add_copper_trace(start, end, pad.GetNet())

	prev_module = module

def layout_triangle():
	startx = 50
	starty = 50
	side = 16
	spacing = 4

	import math
	height = math.sqrt((side*spacing)**2 - (side*spacing/2)**2)

	modules = dict(zip((m.reference for m in board.modules), (m for m in board.modules)))
	from kicad.util.point import Point2D
	for i in range(0, side):
		d = i
		module = modules["D%i"%d]
		place(module, Point2D(startx + spacing*d, starty), 180*10)

		# Connect up the 5V line
		for pad in module._obj.Pads():
			if pad.GetNet().GetNetname() == "+5V":
				start = pcbnew.wxPoint(pad.GetPosition().x, pad.GetPosition().y)
				end = pcbnew.wxPoint(pad.GetPosition().x, pad.GetPosition().y + 1.5)
				add_copper_trace(start, end, pad.GetNet())

			# Connect up the GND line			
			if pad.GetNet().GetNetname() == "GND":
				start = pcbnew.wxPoint(pad.GetPosition().x, pad.GetPosition().y)
				end = pcbnew.wxPoint(pad.GetPosition().x, pad.GetPosition().y - 1.5)
				add_copper_trace(start, end, pad.GetNet())

	for i in range(0, side):
		d = i + side
		module = modules["D%i"%d]
		place(module, Point2D(startx + side * spacing - spacing*i/2., starty + height / side * i), 60*10)

	for i in range(0, side):
		d = i + side * 2
		module = modules["D%i"%d]
		place(module, Point2D(startx + side / 2 * spacing - spacing * i/2., starty + height - height/side * i), 300 * 10)

	# Add 5V and GND tracks
	start = pcbnew.wxPoint(startx + 4.5, starty + 2.5)
	end = pcbnew.wxPoint(startx + side * spacing - 4.5, starty + 2.5)
	add_copper_trace(start, end, netnames["+5V"])

def save():
	print("Saving!")
	backup_path = pcb_path + ".layoutbak"
	if os.path.exists(backup_path):
		os.unlink(backup_path)
	os.rename(pcb_path, backup_path)
	assert board._obj.Save(pcb_path)

if args.delete_all_traces:
	tracks = board._obj.GetTracks()
	for t in tracks:
		print("Deleting track {}".format(t))
		board._obj.Delete(t)
elif args.delete_short_traces:
	print("FIXME: disabled because it removes vias")
	exit(1)
	tracks = board._obj.GetTracks()
	for t in tracks:
		start = t.GetStart()
		end = t.GetEnd()
		length = math.sqrt((end.x - start.x)**2 + (end.y - start.y)**2)
		if length < 100: # millionths of an inch
			print("Deleting trace of short length {}in/1000000".format(length))
			board._obj.Delete(t)
else:
	layout_triangle()

if not args.dry_run:
	save()
