import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('flash',Path(__file__).resolve().parents[1]/'tools/cielo_flash.py')
flash=importlib.util.module_from_spec(spec);spec.loader.exec_module(flash)

def image():
 raw=bytearray(b'\xff'*flash.FLASH_SIZE)
 table=b''
 for typ,sub,offset,size,label in ((1,2,0x9000,0x5000,b'nvs'),(1,0,0xe000,0x2000,b'otadata'),(0,16,0x10000,0x330000,b'app0'),(0,17,0x340000,0x330000,b'app1'),(1,0x82,0x670000,0x180000,b'spiffs'),(1,3,0x7f0000,0x10000,b'coredump')):
  table+=struct.pack('<HBBII16sI',0x50aa,typ,sub,offset,size,label,0)
 table+=b'\xeb\xeb'+b'\xff'*14+hashlib.md5(table).digest()
 raw[0x8000:0x8000+len(table)]=table
 return bytes(raw)

class FlashTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.raw=image()
 def test_exact_size_and_partition_md5(self):
  i=flash.inspect_flash(self.raw);self.assertTrue(i['partition_table_md5_verified']);self.assertEqual(len(i['partitions']),6)
  self.assertRaises(ValueError,flash.inspect_flash,self.raw[:-1])
  bad=bytearray(self.raw);bad[0x800c]^=1;self.assertRaises(ValueError,flash.inspect_flash,bytes(bad))
 def test_overlap_and_oob(self):
  bad=bytearray(self.raw);struct.pack_into('<I',bad,0x8000+32+4,0x9000)
  self.assertRaises(ValueError,flash.inspect_flash,bytes(bad))
 def test_encryption_roundtrip_corruption_wrong_password(self):
  encrypted=flash.seal(self.raw,'test-password-123456','board-hash');raw,info,device=flash.unseal(encrypted,'test-password-123456')
  self.assertEqual(raw,self.raw);self.assertEqual(device,'board-hash');self.assertEqual(info['size'],8388608)
  self.assertRaises(ValueError,flash.unseal,encrypted,'wrong-password')
  self.assertRaises(ValueError,flash.unseal,encrypted[:-1]+bytes([encrypted[-1]^1]),'test-password-123456')
 def test_private_no_overwrite(self):
  with tempfile.TemporaryDirectory() as t:
   path=Path(t)/'copy';flash.private_write(path,b'private')
   self.assertEqual(path.stat().st_mode&0o777,0o600)
   self.assertRaises(FileExistsError,flash.private_write,path,b'wrong')
 def test_double_read_mock_usb_no_write(self):
  outer=self
  class Fake:
   def __init__(self):self.calls=[]
   def device_id(self):return 'device'
   def run(self,cmd,*args,**kwargs):
    self.calls.append((cmd,args,kwargs));Path(args[-1]).write_bytes(outer.raw)
  fake=Fake()
  with tempfile.TemporaryDirectory() as t:
   raw,device=flash.backup(fake,t)
  self.assertEqual(raw,self.raw);self.assertEqual(device,'device')
  self.assertEqual([c[0] for c in fake.calls],['read_flash','read_flash'])
  self.assertEqual(fake.calls[0][2]['after'],'no_reset');self.assertEqual(fake.calls[1][2]['before'],'no_reset')
 def test_mismatched_reads_rejected(self):
  outer=self
  class Fake:
   calls=0
   def device_id(self):return 'device'
   def run(self,cmd,*args,**kwargs):
    self.calls+=1;raw=outer.raw if self.calls==1 else b'\0'*8388608;Path(args[-1]).write_bytes(raw)
  with tempfile.TemporaryDirectory() as t:self.assertRaises(ValueError,flash.backup,Fake(),t)

if __name__=='__main__':unittest.main()
