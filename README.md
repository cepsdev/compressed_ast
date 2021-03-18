# Compressed AST

Overall Problem: To store not strictly typed Abstract Syntax Trees efficiently, i.e. access to data is easy and fast for current processor designs and cache architectures, and
with minimal redundancy, i.e. compression of structural/type information. 
What means "not strictly typed": No a priori type scheme, in XML slang: no schema.

Example:

Assume a json-message which includes a list of points:

msg = {
 "points" : [ 
  {"x": 1.0, "y" : 2.0 },
  {"x": 3.0, "y" : 4.0 },
  {"x": 5.0, "y" : 6.0 }
 ]
}

The actual data is |1.0|2.0|3.0|4.0|5.0|6.0| (where |Double| means a 64 bits wide word) .
That's the way a compiler with knowledge of the structure __msg__ would store the data.

Without knowing the structure of __msg__ in advance - imagine a generic JSON-Library which has to store the data in a way that allows for programmatic access - we are forced to
store structural information together with the actual data, i.e. types and field names:

|"Points"||Type:List||Count:3||OBJECT START||FIELD||"x"||Float||1.0||FIELD||"y"||Float||2.0||OBJECT END||OBJECT START| ... etc.

This is way longer than the former representation.

Strategy: Separate Data ( the |1.0|2.0|3.0|4.0|5.0|6.0| ) from the structural information ( |OBJECT START||FIELD||"x"||Float| etc.). Store the
former like a compiler would - as a sequence of properly aligned fields. Compress the former. Compression scheme below.
  
Compression of ASTs: Multilevel LZW compression of structural information.

Example encoding of __msg__:

__Data Segment__: |1.0|2.0|3.0|4.0|5.0|6.0|

Encoding of the __structural information__ (Type Segment):

1. __msg__ has the folowing (uncompressed structure, D means DOWN, U means UP : D starts an object U ends it):

"points" D <br/>
 &nbsp;D <br/>
 &nbsp;&nbsp;"x"  1.0 <br/>
 &nbsp;&nbsp;"y"  2.0 <br/>
 &nbsp;U <br/>
 &nbsp;D <br/>
 &nbsp;&nbsp;"x"  3.0 <br/>
 &nbsp;&nbsp;"y"  4.0 <br/>
 &nbsp;U <br/>
 &nbsp;D <br/>
 &nbsp;&nbsp;"x"  5.0 <br/>
 &nbsp;&nbsp;"y"  6.0 <br/>
 &nbsp;U <br/>
U <br/>

The structure is (the data is stored separately):

"points" D <br/>
 &nbsp;D <br/>
 &nbsp;&nbsp;"x" <br/>
 &nbsp;&nbsp;"y" <br/>
 &nbsp;U <br/>
 &nbsp;D <br/>
 &nbsp;&nbsp;"x"  <br/>
 &nbsp;&nbsp;"y"  <br/>
 &nbsp;U <br/>
 &nbsp;D <br/>
 &nbsp;&nbsp;"x"  <br/>
 &nbsp;&nbsp;"y"  <br/>
 &nbsp;U <br/>
U <br/>

2. Recursively (starting with the leaves, going upwards(postorder traversal) ) apply dictionary- and RLE-compression:  

 "x" | <br/>
 "y" |==> N1 <br/>
 <br/>
 ==><br/>

 "points" D <br/>
  &nbsp;D <br/>
  &nbsp;&nbsp;N1  <br/>
  &nbsp;U <br/>
  &nbsp;D <br/>
  &nbsp;&nbsp;N1 <br/> 
  &nbsp;U <br/>
  &nbsp;D <br/>
  &nbsp;&nbsp;N1 <br/>
  &nbsp;U <br/>
U <br/>
<br/>
 D    |<br/>
 &nbsp;N1  | <br/>
 U    | => N2<br/>
<br/>
==><br/>
<br/>
 "points" <br/> 
  &nbsp;D <br/>
  &nbsp;&nbsp; 3 __*__ N2 <br/>
  &nbsp;U <br/>
 <br/>
==> N3 <br/>
<br/>
With the resulting Type-Table:<br/>
<br/>
[
 N1 = "x","y",<br/>
 N2 = D N1 U,<br/>
 N3 = "points" D 3*N2 U<br/>
]
