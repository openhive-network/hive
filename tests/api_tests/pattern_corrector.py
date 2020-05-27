   #!/usr/bin/python3

import os
import sys
import json

class PatternCorrector( object ):

   def __init__( self, paths ):

      self.location = []
      for path in paths:
         self.location.append( { "level" : 0, "last": 0 } )

      self.paths = paths
      self.undefined_value = "UNDEFINED-VALUE-FOR-API-TEST"


   def json_pretty_string(self, json_obj):
      return json.dumps(json_obj, sort_keys = True, indent = 2 )

   def move_paths( self, key, is_forward ):
      cnt = 0

      for path in self.paths:

         if is_forward:
            _len = self.location[ cnt ][ "level" ]
            if( _len >= len( path ) ):
               hit = False
            else:
               hit = ( path[ self.location[ cnt ][ "level" ] ] == key )
            if hit:
               self.location[ cnt ][ "last" ] += 1
            self.location[ cnt ][ "level" ] += 1
         else:
            self.location[ cnt ][ "level" ] -= 1
            _len = self.location[ cnt ][ "level" ]
            if( _len >= len( path ) ):
               hit = False
            else:
               hit = ( path[ self.location[ cnt ][ "level" ] ] == key )
            if hit:
               self.location[ cnt ][ "last" ] -= 1

         cnt += 1

   def forward_paths( self, key ):
      self.move_paths( key, True )

   def backward_paths( self, key ):
      self.move_paths( key, False )

   def try_to_change( self, data ):
      cnt = 0

      for path in self.paths:

         if self.location[ cnt ][ "level" ] == self.location[ cnt ][ "last" ]:
            return self.undefined_value

         cnt += 1

      return data
   
   def process_array( self, data ):
      assert isinstance( data, list )

      res = []
      for item in data:
         res.append( self.choice( item ) )

      return res

   def process_dict( self, data ):
      assert isinstance( data, dict )

      keys = []
      values = []
      for key, val in data.items():

         self.forward_paths( key )
         keys.append( key )
         values.append( self.choice( val ) )
         self.backward_paths( key )

      merged = zip( keys, values )
      return dict( merged )

   def choice( self, data ):
      if isinstance( data, list ):
         return self.process_array( data )
      elif isinstance( data, dict ):
         return self.process_dict( data )
      else:
         return self.try_to_change( data )

   def run( self, data ):
      return self.choice( data )
