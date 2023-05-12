<?xml version="1.0"?>


<!--
 * Copyright (C) 2017-2023 by Erik Hofman.
 * Copyright (C) 2017-2023 by Adalin B.V.
 * All rights reserved.
 *
 * This file is part of AeonWave and covered by the
 * Creative Commons Attribution-ShareAlike 4.0 International Public License
 * https://creativecommons.org/licenses/by-sa/4.0/legalcode
-->

<aeonwave>


 <info>
  <license type="Attribution-ShareAlike 4.0 International"/>
  <copyright from="2017" until="2023" by="Adalin B.V."/>
  <copyright from="2017" until="2023" by="Erik Hofman"/>
  <contact author="Erik Hofman" website="aeonwave.xyz"/>
 </info>

 <audioframe mode="append">
  <effect type="delay" src="false">
   <slot n="0">
    <param n="0">0.50</param>
    <param n="1">0.0</param>
    <param n="2">0.0</param>
    <param n="3" type="msec">340.0</param>
   </slot>
   <slot n="1">
    <param n="0">0.0</param>
    <param n="1">0.0</param>
    <param n="2">0.250</param>
    <param n="3">1.0</param>
   </slot>
  </effect>
 </audioframe>

 <mixer mode="append">
  <effect type="delay" src="false">
   <slot n="0">
    <param n="0">0.50</param>
    <param n="1">0.0</param>
    <param n="2">0.0</param>
    <param n="3" type="msec">340.0</param>
   </slot>
   <slot n="1">
    <param n="0">0.0</param>
    <param n="1">0.0</param>
    <param n="2">0.250</param>
    <param n="3">1.0</param>
   </slot>
  </effect>
 </mixer>

</aeonwave>
