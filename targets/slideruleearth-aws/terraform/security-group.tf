# Security Group for Monitor
resource "aws_security_group" "monitor-sg" {
  vpc_id       = aws_vpc.sliderule-vpc.id
  name         = "${var.cluster_name}-monitor-sg"
  description  = "Monitor Security Group"
  tags = {
    Name = "${var.cluster_name}-monitor-sg"
  }

  # Loki - TCP
  ingress {
    cidr_blocks = [var.vpcCIDRblock]
    from_port   = 3100
    to_port     = 3100
    protocol    = "tcp"
  }

  # HAProxy - Grafana - TCP
  ingress {
    cidr_blocks = [var.vpcCIDRblock]
    from_port   = 3000
    to_port     = 3000
    protocol    = "tcp"
  }

  # Outbound - ALL
  egress {
    cidr_blocks = [var.publicCIDRblock]
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
  }
}

# Security Group for Intelligent Load Balancer
resource "aws_security_group" "ilb-sg" {
  vpc_id       = aws_vpc.sliderule-vpc.id
  name         = "${var.cluster_name}-ilb-sg"
  description  = "Intelligent Load Balancer Security Group"
  tags = {
    Name = "${var.cluster_name}-ilb-sg"
  }

  # HAProxy - HTTPS
  ingress {
    cidr_blocks = [var.publicCIDRblock]
    from_port   = 443
    to_port     = 443
    protocol    = "tcp"
  }

  # Orchestrator - TCP
  ingress {
    cidr_blocks = [var.vpcCIDRblock]
    from_port   = 8050
    to_port     = 8050
    protocol    = "tcp"
  }

  # Node Exporter (from Prometheus) - TCP
  ingress {
    cidr_blocks = [var.vpcCIDRblock]
    from_port   = 9100
    to_port     = 9100
    protocol    = "tcp"
  }

  # Outbound - ALL
  egress {
    cidr_blocks = [var.publicCIDRblock]
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
  }
}

# Security Group for Nodes
resource "aws_security_group" "node-sg" {
  vpc_id       = aws_vpc.sliderule-vpc.id
  name         = "${var.cluster_name}-node-sg"
  description  = "Node Security Group"
  tags = {
    Name = "${var.cluster_name}-node-sg"
  }

  # SlideRule Node (from ILB) - TCP
  ingress {
    cidr_blocks = [var.vpcCIDRblock]
    from_port   = 9081
    to_port     = 9081
    protocol    = "tcp"
  }

  # SlideRule Node (from Prometheus) - TCP
  ingress {
    cidr_blocks = [var.vpcCIDRblock]
    from_port   = 10081
    to_port     = 10081
    protocol    = "tcp"
  }

  # Node Exporter (from Prometheus) - TCP
  ingress {
    cidr_blocks = [var.vpcCIDRblock]
    from_port   = 9100
    to_port     = 9100
    protocol    = "tcp"
  }

  # Outbound - ALL
  egress {
    cidr_blocks = [var.publicCIDRblock]
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
  }
}
