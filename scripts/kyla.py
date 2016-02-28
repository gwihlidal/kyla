try:
	from lxml import etree
except ImportError:
	import xml.etree.ElementTree as etree

import os
import uuid
import xml.dom.minidom

def _IdString (s):
	if isinstance (s, uuid.UUID):
		return str (s).upper ()
	else:
		return s

from enum import Enum

class PackageType(Enum):
	Loose = 0
	Packed = 1
	Bundle = 2

class FileRepositoryBuilder:
	def __init__ (self, name=None, version=None):
		self._propertyNode = etree.Element ('Properties')
		self._fileSets = []

	def SetPackageType(self, packageType):
		property = etree.SubElement (self._propertyNode, 'Property')
		property.set ('Name', 'PackageType')
		property.set ('Value', packageType.name)

	class FileSetBuilder:
		def __init__ (self, name, fileSetId = None):
			if fileSetId is None:
				fileSetId = uuid.uuid4 ()

			self.__element = etree.Element ('FileSet')
			self.__element.set ('Name', name)
			self.__element.set ('Id', _IdString (fileSetId))

		def SetSourcePackage (self, sourcePackage):
			self.__element.set = ('SourcePackage', sourcePackage)

		def AddFilesFromDirectory (self, baseDirectory, prefix=''):
			for directory, _, entry in os.walk (baseDirectory):
				directory = directory [len (baseDirectory) + 1:]
				if entry:
					for e in entry:
						fileElement = etree.SubElement (self.__element, 'File')
						fileElement.set ('Source', os.path.join (prefix, directory, e))

		def Get(self):
			return self.__element

	def AddFileSet (self, name = None, fileSetId = None):
		if fileSetId is None:
			fileSetId = uuid.uuid4 ()

		fb = self.FileSetBuilder (name, fileSetId)
		self._fileSets.append (fb)
		return fb

	def Finalize (self, prettyPrint = True):
		'''Generate the installer XML and return as a string. The output is
		already preprocessed.'''
		root = etree.Element ('FileRepository')
		root.append (self._propertyNode)

		fileSets = etree.SubElement (root, 'FileSets')
		for fs in self._fileSets:
			fileSets.append (fs.Get ())

		decl = '<?xml version="1.0" encoding="UTF-8"?>'
		result = decl + etree.tostring (root, encoding='utf-8').decode ('utf-8')

		if prettyPrint:
			d = xml.dom.minidom.parseString (result)
			return d.toprettyxml ()
		else:
			return result
