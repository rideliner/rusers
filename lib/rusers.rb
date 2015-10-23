
require 'rusers/version'
require 'rusers/rusers'

module RemoteUsers
  MACHINES_FILE = File.join(Dir.home, '.machines')

  class Machine < Struct.new(:name, :room, :os, :use)
    def to_s
      "#{name}#{room.nil? ? '' : "@#{room}"}"
    end

    def ==(other)
      name == other.name
    end

    def <=>(other)
      name <=> other.name
    end

    include Comparable
  end

  class Connection
    def initialize(hosts)
      @hosts = if hosts.is_a?(Enumerable)
        hosts.map { |x| mapToMachine(x) }
      else
        mapToMachine(hosts)
      end
    end

    def mapToMachine(str)
      if str.is_a?(Machine)
        str
      else
        Machine.new str, nil, nil, nil
      end
    end

    def getHostsInfo
      return to_enum(:getHostsInfo) unless block_given?

      if @hosts.is_a?(Enumerable)
        group = ThreadGroup.new
        @hosts.each { |host|
          t = Thread.new do
            begin
              yield host => countUsers(getHostInfo(host.name))
            rescue RPCException
              yield host => nil
            end
          end
          group.add(t)
        }

        group.list.each { |x|
          unless x.nil?
            x.join 0.1
          end
        }
      else
        yield @hosts => countUsers(getHostInfo(@hosts.name))
      end
    end

    def countUsers(arr)
      arr.group_by { |x| x.username }.map { |_, values|
        [ values.first, values.length ]
      }
    end

    private :getHostInfo, :countUsers, :mapToMachine

    def findUser(users, exactNames = true)
      return to_enum(:findUser, users, exactNames) unless block_given?

      reg = users.is_a?(Array) ? users.map { |x| "(?:#{x})" }.join('|') : users.to_s
      reg = ".*#{reg}.*" unless exactNames
      reg = /^#{reg}$/

      getHostsInfo { |info|
        host, users = info.shift

        unless users.nil?
          users.each { |user|
            yield host => user if user.first.username =~ reg
          }
        end
      }
    end
  end
end
