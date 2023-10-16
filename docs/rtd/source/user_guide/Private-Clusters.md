# Private Clusters

2023-02-24

## Background

With release v2.0.0, SlideRule supports private clusters - clusters that can only be accessed by authenticated users affiliated with the cluster.  Prior to v2.0.0, all of SlideRule's services were provided by a single cluster of nodes executing in AWS us-west-2, available publicly to anyone on the internet. This makes SlideRule's services easy to access, yet suffers when many users try to access SlideRule at the same time.  The SlideRule team can only allocate a certain level of funding towards a public cluster and cannot satisfy the processing needs of everyone at once.

For users of SlideRule that have access to their own funding, private clusters provide a way to have access to all of SlideRules services without having to share the processing power behind those services with other users or organizations.  In order to manage private clusters and authenticate users, the SlideRule team developed a system called the SlideRule Provisioning System at [https://ps.slideruleearth.io](https://ps.slideruleearth.io).  The SlideRule Provisioning System provides the following functionality:

* Deploys and destroys SlideRule clusters
* Manages and authenticates users
* Generates temporary credentials needed to access a private SlideRule cluster
* Maintains a custom budget for each private cluster
* Tracks the AWS accrued cost of a private cluster and will both prevent a cluster from starting if insufficient funds are available and automatically shuts down a private cluster when funds have been exhausted
* Implements automatic sleeping and waking functions for a cluster in order to save costs


## Getting Started with Private Clusters

1. Create an account on the [SlideRule Provisioning System](https://ps.slideruleearth.io).

2. If you are already associated with an organization that has a private cluster on slideruleearth.io, then request membership to your organization using the Provisioning System.  If you want to create your own organization, please reach out to the SlideRule science team via one of the methods described on our [Contact Us](https://slideruleearth.io/contact/) page.

3. Set up a __.netrc__ file in your home directory with the following entry:
```
machine ps.slideruleearth.io login {your_username} password {your_password}
```

4. Modify your Python scripts to supply your organization in the call to initialize the client, like so:
```Python
icesat2.init("slideruleearth.io", organization="{your_organization}")
```

## Starting and Scaling a Private Cluster

A private cluster is configured by the manager/owner of the cluster with a minimal number of nodes.  If the minimal number of nodes is unsufficent for the processing the user wants to accomplish, the user can temporarily increase the number of nodes running for a user-defined amount of time, up to a maximum number of nodes set by the owner.  This is called scaling the cluster.  In some cases, the cluster is configured with a minimal number of nodes of zero.  In this case, anyone wishing to use the cluster must first scale the cluster to at least one node before they are able to use it.

Scaling a cluster can be accomplished a few different ways depending on the needs of the user and whether or not the cluster is at zero nodes (fully shutdown) or not.
* If you want to guarantee that the cluster is up and that a minimal number of nodes is running before you do any data processing in your script, you can specify the desired number of nodes and a time to live in the `sliderule.init` call.  For example:
```Python
sliderule.init("slideruleearth.io", organization="{your_organization}", desired_nodes=5, time_to_live=60) # run 5 nodes for 60 minutes
```
* If you want to make a non-blocking request to increase the number of nodes in the cluster, then you want to use `sliderule.update_available_servers`.  This will allow you to keep using the cluster while more nodes are started.  (Of course, if the cluster is not up at all, then subsequent requests to the cluster will fail until it all comes up). For example:
```Python
sliderule.update_available_servers(desired_nodes=5, time_to_live=60) # kick off starting 5 nodes for 60 minutes
```
* If you want to make a blocking request to increase the number of nodes in the cluster, then you want to use `sliderule.scaleout`.  This is the call that `sliderule.init` makes underneath, which means that using the `init` family of functions with the `desired_nodes` argument set, will also block.
```Python
sliderule.scaleout(desired_nodes=5, time_to_live=60) # request and wait for 5 nodes for 60 minutes
```

## Troubleshooting

* **Version Incompatibilities**: Private clusters run a pinned version of the sliderule server code which is determined by the owner of the cluster.  If your client is on a different version than the server, please work with the owner of your cluster to resolve which client version you should be running.  Note that version incompatibilities will be reported by the client on initialization like so:
```
RuntimeError: Client (version (4, 0, 2)) is incompatible with the server (version (3, 7, 0))
```

* **Cluster Not Started**: Private clusters can be configured to completely shutdown when not in use.  When that happens, one of the scaling techniques above must be used to start the cluster.  When this happens, users will see a message like:
```
Connection error to endpoint https://{your_organization}.slideruleearth.io/source/version ...retrying request
```
