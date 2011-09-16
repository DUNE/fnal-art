---
title: The Event Data Model
next: /dataproducts
prev: /concepts
---

EDM: the Event Data Model
=========================

* Library for organizing, navigating, and persisting data processed by the experiment

* Organization is hierarchical: *Run* / *SubRun* / *Event*

* Processing happens one Event at a time

* The EDM at the lowest level appears as a small database holding data
  products generated (collected or derived) for the associated data
  taking period

* Modules perform queries to find existing data objects and do inserts
  to store new data products associated with this Event